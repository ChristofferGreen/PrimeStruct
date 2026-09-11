# Receiver-Target Resolution Consolidation Plan

Status: Step 2, six call sites migrated (2026-09-08/09), plus a seventh
call site (F12) harnessed for observation but not yet migrated -
`resolveArgsPackElementMethodTarget`
(`SemanticsValidatorMethodTargetArgsPackResolvers.cpp`, Step 1b's slice 1
call site), `resolveMethodTarget`'s own inline indexed-args-pack-element
cascade (`SemanticsValidatorExprMethodTargetResolution.cpp`, slice 2 - the
`pack[i].method()` access shape), and monomorphization's
`resolveMethodCallTemplateTarget`'s F11 FileError sub-case, F9 primitive
slice, F13/F13b/F13c collection-family slice, and F7 File-family slice
(all four in `TemplateMonomorphMethodTargets.cpp`) now all delegate for
real to the joint `(type, methodName, templateShape)` classifier
(`classifyReceiverElementFamilyJoint`) instead of their own inline
R1-R9/F11/F9/F13/F7-shaped checks; the diff-audit harness at each call
site is retired (superseded by the real migration - diffing a classifier
against itself is meaningless). See "Step 2: resolveArgsPackElementMethodTarget
migrated to classifyReceiverElementFamilyJoint, zero-divergence achieved
(2026-09-08)", "Step 2, second migration: resolveMethodTarget's
indexed-args-pack cascade migrated to classifyReceiverElementFamilyJoint,
zero-divergence achieved (2026-09-09)", "Step 2, monomorphization
stage: resolveMethodCallTemplateTarget's F11 FileError sub-case migrated
to classifyReceiverElementFamilyJoint, zero-divergence achieved
(2026-09-09)", "Step 2, monomorphization stage:
resolveMethodCallTemplateTarget's F9 primitive slice migrated to
classifyReceiverElementFamilyJoint, zero-divergence achieved (2026-09-09)",
"Step 2, monomorphization stage: resolveMethodCallTemplateTarget's
F13/F13b/F13c collection-family slice migrated to
classifyReceiverElementFamilyJoint, zero-divergence achieved (2026-09-09)",
and "Step 2, monomorphization stage: resolveMethodCallTemplateTarget's F7
File-family slice migrated to classifyReceiverElementFamilyJoint,
zero-divergence achieved (2026-09-09)" below for the full detail and
verification proof of each.
All four monomorphization diff-audit harnesses (F11, F9, F13/F13b/F13c,
and F7) are now fully retired - no diff-audit scaffolding remains
anywhere in `TemplateMonomorphMethodTargets.cpp`.
Monomorphization now has four call sites (F9, F11, F13/F13b/F13c, F7)
delegating to the shared classifier - two stages (semantics,
monomorphization) now each have at least one call site delegating to the
shared classifier; `ir_lowerer` remains untouched.
A fifth monomorphization diff-audit harness, F12 (generic-Soa-receiver
method-name-paired dispatch), was landed and verified zero-divergence
2026-09-09 - see "Step 1b, monomorphization stage: fifth diff-audit
harness at F12 generic-Soa slice, zero-divergence verified, migration
deferred" below; unlike the other five harnesses it is not yet migrated
for real (deliberately deferred to a future Step 2 round), so it is the
one diff-audit harness still present in `TemplateMonomorphMethodTargets.cpp`.
Remaining scope in Row F: 12 of its 17 branches (F0-F6/F8, F10, F14-F16)
plus F12's still-open real migration - see "Step 1b, monomorphization
stage: fourth diff-audit harness at F7 File-family slice" below for why
every other still-unmigrated Row F branch either needs no classifier work
(trivial guards), needs a broader interface extension than F7 did, or was
already ruled a non-fit in a prior round.
Step 0 (characterize the full rule table) is otherwise still in
progress - see "Step 0 Rule Table" below;
semantics-stage method-target resolvers, all five snapshot-collection
mechanisms, and monomorphization are now fully branch-enumerated; the
`ir_lowerer` stage's own `resolveMethodCallDefinitionFromExpr` (Row G) is
now enumerated too, but its two sibling receiver-target-helper/collection-
helper files remain open. This is the sibling
problem `docs/CompatPathResolutionConsolidation.md` explicitly deferred as
a non-goal: "Method-call *receiver* inference (which type a method call
dispatches on) stays where it is; the classifier only decides spelling
disposition, not receiver typing." That document's own Risks section
predicted this: "If Step 0 uncovers a third layer of the same shape, that
is a signal to stop and reassess." This is that third layer.
Step 1b for `ir_lowerer` was started 2026-09-09: the two candidates most
analogous to the already-migrated File/Buffer/FileError slices (G6's
bare-`Name` static-error dispatch, and the Receiver/Collection-helper
files' unconditional Buffer/File `LocalInfo`-kind assignments) were both
checked and turned out to be the already-excluded F1 and F3 shapes
respectively, not clean fits - see "Step 1b, ir_lowerer stage: candidate
branches assessed, none fit the classifier this round" below. No harness
wired, no source changed; broader Row G/RT/CH scope remains open for a
future round.

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

### Step 1b — Full classifier plus differential-audit harness (started 2026-09-08, one call site)

Extended the Step 1a module with `classifyReceiverElementFamilyJoint`,
taking the `(type, methodName, templateShape)` joint inputs Row category
A's rule table documents, and wired an env-gated
(`PRIMESTRUCT_RECEIVER_TARGET_DIFF_AUDIT=1`) differential-audit harness
into `resolveArgsPackElementMethodTarget` - see the dedicated Update
section below for the full detail (classifier design, wiring mechanics,
zero-divergence proof, unchanged-default-behavior proof).

A second round (slice 2, same day) wired the identical harness pattern
into a second call site, `resolveMethodTarget`'s own inline indexed-
args-pack-element cascade (the `pack[i].method()` access shape) - a
near-duplicate of slice 1's cascade discovered while looking for the
next well-scoped call site. No classifier code changed; only the two
inputs' relationship differs per call site (see the dedicated "Step 1b
slice 2" section below). Remaining scope: every other Row A/B/C/D/E call
site (`resolveMethodCallTemplateTarget` and siblings in monomorphization,
`ir_lowerer`'s own helpers, the snapshot-collection mechanisms, and the
still-sprawling Row B/C functions - `classifyExplicitVectorHelperReceiver`,
`resolveBorrowedVectorReceiver`, `preferredKeyValueMethodTarget`, the five
independent "is this a map receiver" implementations) still independently
re-derives receiver family membership - this round wired exactly one more
function, deliberately, per this document's own staged-rollout
discipline.

### Step 2 — Migrate stage by stage (started 2026-09-08; monomorphization's first call site landed 2026-09-09)

Same order and discipline as the compat-spelling consolidation: semantics
first (reference behavior), then monomorphization, then `ir_lowerer`, each
gated on zero-divergence across `PrimeStruct_semantics_tests`,
`PrimeStruct_backend_ir_tests`, and `PrimeStruct_compile_run_tests`. First
real migration (`resolveArgsPackElementMethodTarget`, Row category A's
entry point) landed 2026-09-08 - see the dedicated "Step 2" section below
for the full detail and verification proof. Second migration
(`resolveMethodTarget`'s indexed-args-pack cascade, Step 1b slice 2) landed
2026-09-09 - see the dedicated "Step 2, second migration" section below.
Both Step 1b-harnessed semantics-stage call sites are now migrated. A third
migration, the first in the monomorphization stage
(`resolveMethodCallTemplateTarget`'s F11 FileError sub-case) landed the
same day - see the dedicated "Step 2, monomorphization stage" section
below. A fourth migration, monomorphization's F9 primitive slice, landed
the same day too - see the dedicated "Step 2, monomorphization stage:
resolveMethodCallTemplateTarget's F9 primitive slice migrated..." section
below. A fifth migration, monomorphization's F13/F13b/F13c
collection-family slice, landed the same day too - see the dedicated
"Step 2, monomorphization stage: resolveMethodCallTemplateTarget's
F13/F13b/F13c collection-family slice migrated..." section below. A
sixth migration, monomorphization's F7 File-family slice, landed the
same day too - see the dedicated "Step 2, monomorphization stage:
resolveMethodCallTemplateTarget's F7 File-family slice migrated..."
section below.
Remaining scope is every other Row A/B/C/D/E/F/G call site this
document's Step 0 rule table catalogs, including 13 of Row F's 17
branches (F0-F6/F8, F10, F12, F14-F16) and all of `ir_lowerer`. Both
stages' remaining scope, as of the two exhaustive Step 1b sweeps
(monomorphization's Row F and `ir_lowerer`'s Row G/RT/CH, both
2026-09-09), turned out to be receiver-type-inference shaped (F3;
RT2/RT3/G7), not classifier-shaped - see Step 1c below, which refines
where that remaining scope goes rather than reopening Step 2 for the
classifier itself.

### Step 1c — Separate receiver-type inference from receiver-family classification (scoping started 2026-09-10; no code yet)

Refines, does not contradict, Step 1b/Step 2 above: those steps'
`(type, methodName, templateShape) -> family` classifier
(`classifyReceiverElementFamilyJoint`) and its 8 landed migrations stay
exactly as delivered and are not touched here. Step 1c is new scope for
the receiver-type-*inference* question (F3 in monomorphization;
RT2/RT3/G7 in `ir_lowerer`) that both stages' exhaustive Step 1b sweeps
found the classifier structurally cannot absorb without recreating the
entanglement this document exists to untangle - a new, canonical
receiver-type output shape (`CanonicalReceiverType`) that a new
per-stage `resolveReceiverType(<stage-specific input>, stage-context)`
converges on producing, feeding into (not replacing)
`classifyReceiverElementFamilyJoint` for the family verdict. This
round's task was Step 0-style characterization only for this new
module - no `CanonicalReceiverType`/`resolveReceiverType` implementation,
no production code touched. See the dedicated "Step 1c Scoping" section
below for the field-by-field F3/RT2/RT3/G7 mapping, the one
irreconcilable case found (F3-C3a, which fuses resolution into what is
otherwise a pure inference cascade and must stay outside this design
entirely), and one deliberately-unresolved open question (whether
args-pack storage facts belong in `CanonicalReceiverType`'s output or in
`resolveReceiverType`'s stage-specific input). A future round implements
`CanonicalReceiverType`/`resolveReceiverType` proper, grounded in this
scoping rather than guessing at the interface.

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
| F14 | **DELETED** (2026-09-09). Was: `ctx.sourceDefs` **does** have a definition at `resolvedType`, AND it is specifically an experimental-SOA-*specialized* type path (`isConcreteExperimentalSoaReceiver`), AND method matches one of the same 4 pairs/`push`/`reserve` as F12 | Was: dispatch via the same `preferredSamePath*MethodTarget` helpers as F12, but **without ever consulting `isBorrowedSoaReceiver`/`borrowedSoaWrapperMethodName`** - the borrowed-vs-owned renaming F12 applies for the generic (not-yet-concrete) SOA case is silently skipped once the receiver resolves to a concrete experimental-SOA type | N/A - branch removed. Confirmed unreachable dead code (proof re-derived against F12's post-migration, classifier-based guard - see "F14 deleted" section near the end of this document) and deleted outright rather than migrated; the "asymmetry" this row used to describe was never observable at runtime because F12 always returns first for every input that could also satisfy F14's guard. |
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

## Step 0 UNPINNED test-coverage cross-reference (2026-09-08, sixth round)

Continued the fifth round's cross-reference pass, working from its own
"Still open — not attempted this round" list, prioritizing (per this
task's own instructions) any remaining "live, not latent" candidates and
the most heavily-duplicated "what type family is this receiver"
predicates first. Same methodology: `grep tests/unit/` for the exact
function/behavior names first, then hand-built `.prime` repros compiled
with `--dump-stage semantic-product` (the fifth round's methodology
section already establishes this; not repeated in full here) via the
pre-built `build-release/primec` binary where grep alone couldn't settle
reachability. No source file was modified; all repro `.prime` files were
written under the session scratchpad and never copied into the repo
(confirmed via `git status`/`git diff --stat` before committing). **8
additional UNPINNED rows/sub-guards were audited this round** (fewer than
the fifth round's 14, by design — several of this round's rows required
multi-step source tracing rather than a single grep/repro, and depth was
prioritized over breadth per this task's own budget guidance). The
remaining ~46 UNPINNED rows are listed at the end of this section for a
future round.

### Headline finding: Row F's F12/F14 SOA "asymmetry" is not an
asymmetry — F14 is dead code, confirmed by direct source reading

The fourth/fifth rounds' rule table described F12
(`isTemplateMonomorphSoaReceiverType`-gated dispatch for the generic,
not-yet-concrete SOA receiver case, `TemplateMonomorphMethodTargets.cpp`
lines 604-646) and F14 (`isConcreteExperimentalSoaReceiver`-gated
dispatch for the concrete/specialized-experimental-SOA case, lines
671-704) as "a genuine asymmetry between F12 and F14 for what should be
the same logical distinction (borrowed vs. owned SOA receiver) ...  not
confirmed whether any real borrowed-and-concrete-experimental-SOA
receiver shape is reachable to expose it." Tracing this fully this round
found something stronger than an asymmetry: **F14 can never execute at
all.**

- `normalizedTypeName` (`TemplateMonomorphMethodTargets.cpp:532-539`) is
  computed once and never reassigned between F12 and F14.
- F12's guard (line 604 et seq.) is exactly
  `isTemplateMonomorphSoaReceiverType(normalizedTypeName)` combined with
  a method-name check, for the **identical** six method-name pairs F14
  also checks: `count`/`count_ref`, the two `to_aos` spellings,
  `get`/`get_ref`, `push`/`reserve`, `ref`/`ref_ref`.
- F14's guard (line 671-673) is
  `isTemplateMonomorphSoaReceiverType(normalizedTypeName) &&
  isExperimentalSoaVectorSpecializedTypePath(resolvedType)` — a strict
  superset condition (everything F12 requires, plus one more predicate)
  — over the **same** six method-name pairs.
- Every one of F12's six branches (lines 604, 613, 623, 632, 638)
  unconditionally `return true` once its guard matches. Since F14's guard
  implies F12's guard for every method name F14 handles, execution can
  never reach line 671 with a matching method name that F12 hasn't
  already dispatched and returned from three-dozen-plus lines earlier in
  the same function.
- Confirmed `isExperimentalSoaVectorSpecializedTypePath` has no other
  call site in this file (`grep -n` for it: one hit, line 673) that could
  somehow gate entry to this block differently — there is no earlier
  branch between F12 and F14 that could exit before F14 for a
  concrete-experimental receiver specifically (read lines 555-671 in
  full: FileError/ImageError/ContainerError/GfxError static-dispatch
  branches only, none SOA-related, none returning early for this
  shape).
- `isTemplateMonomorphSoaReceiverType` itself
  (`TemplateMonomorphCoreUtilities.cpp:42-44`) is a single string-equality
  check against a fixed `"soa"`-normalized constant — it does not
  distinguish generic-vs-concrete-specialized receivers at all, which is
  exactly why F12's guard is a superset: both a generic `soa<T>` and a
  concrete experimental-SOA-specialized receiver normalize to the same
  `normalizedTypeName`.

This resolves the "not confirmed whether reachable" question definitively
via code reading alone, no repro needed: F14 (lines 671-704, the
`isConcreteExperimentalSoaReceiver`-gated block, `count`/`count_ref` at
674-679, `get`/`get_ref` at 680-685, `push`/`reserve` at 686-691,
`ref`/`ref_ref` at 692-697, `to_aos` variants at 698-704) is unreachable
dead code, not merely under-tested — the extensive borrowed/owned SOA
test corpus in
`test_compile_run_vm_collections_wrapper_temporaries_reject_count_soa_experimental_runs_borrowed.cpp`
(60+ TEST_CASEs covering exactly this borrowed-vs-owned `count`/`get`/
`ref`/`to_aos`/`push`/`reserve` surface across generic `SoaVector<T>`,
`Reference<SoaVector<T>>`, helper-return, and struct-method-borrowed
receiver shapes, all passing) is consistent with this: every one of those
cases is actually being served by F12 alone, and F14's
`isBorrowedSoaReceiver`-blind dispatch is never exercised because it
never runs. Whether this is deliberate (F14 kept as defensive/
future-proofing code, or historical residue from before F12's guard was
broadened to subsume it) is undetermined — no in-source comment explains
it — but it is definitively not a live behavioral divergence for any
input, borrowed or owned, generic or concrete-specialized. This closes
the fourth/fifth rounds' open question on this row with more certainty
than a coverage grep could have provided (a differential-audit harness
built for Step 1b would need to know this branch cannot fire at all,
rather than trying to construct a test case that reaches it).

### Confirmed genuinely zero test coverage (new this round)

- **R13/H6** (`collection_specializations`: a nested-collection binding's
  outer classification succeeds but `elementTypeText` is left as the raw,
  unexpanded nested-template text rather than being recursively
  reclassified). Confirmed reachable — contrary to what the fifth round's
  adjacent finding about literal `vector<vector<i32>>()` *construction*
  being rejected upstream ("collection literal requires
  relocation-trivial collection element type") might suggest — via a
  **parameter declaration alone** (no construction): `useit([vector<vector<i32>>]
  values) { ... }` compiles past semantics validation and its dump shows
  `collection_specializations[0]: ... collection_family="vector"
  binding_type_text="vector<vector<i32>>" element_type_text="vector<i32>"
  value_type_text="vector<i32>"` — exactly the un-expanded-nested-text
  behavior the rule table's H6 row describes, and genuinely reachable for
  a real (if unusual) parameter type. Grepped the whole `tests/` tree for
  `vector<vector<` — zero hits anywhere — so this reachable branch has no
  test coverage at all, not just an absence of edge-case assertions.
- **R13/H2's positive `Pointer<...>`-wrapped shape** (as opposed to the
  fifth round's already-established zero-coverage finding for this same
  row, which was grep-only). This round extends that finding with a
  reachability proof: `useit([Pointer<vector<i32>>] values) { ... }`
  compiles cleanly and its `collection_specializations` dump entry shows
  `is_pointer=true is_reference=false collection_family="vector"
  element_type_text="i32"` — i.e. the `Pointer<...>` unwrap path works
  correctly, symmetric with the already-tested `Reference<...>` case, it
  is simply never exercised by any test (every existing `isPointer`
  assertion in the corpus's one relevant test file is `CHECK_FALSE`, per
  the fifth round's own grep). Confirms this is a live, reachable,
  correctly-implemented gap in coverage, not a latent one.
- **E10** (`direct_call_targets`: a call whose `resolvedPath` stays empty
  after E2-E4 gets silently no entry, rather than being recorded as
  absent-or-unresolved). Grepped
  `tests/unit/compile_run/test_compile_run_benchmark_harness.cpp` (the
  file exercising `direct_call_targets` most directly) for every
  `direct_call_targets` reference — all of them assert positive presence
  or a positive count (`direct_call_targets[0]`/`[1]`, `counts.get('direct_call_targets')
  == 2`, `== 4`, etc.); none assert that a specific call is *absent* from
  the collector, or that the collector's count stays lower than the
  source's total call count when some calls should fail to resolve. This
  confirms the rule table's own note ("no test asserts a specific call is
  absent from this collector; only positive-presence assertions found")
  — the concept of "a call silently vanishes from this fact family
  instead of erroring" is untested by name anywhere in the corpus.

### Resolved as latent-only (confirmed unreachable via upstream semantics
validation, not merely under-tested)

Five more rows resolve the same way the fifth round's F11-eof/F1-not
pair did: a real gap exists in the row's own code, but the shape needed
to reach it is rejected by semantics-stage validation before the
row's own function ever runs, for any program that compiles far enough
to reach it.

- **R13/H3b** (`vector`-base collection-specialization with template-arg
  count != 1). Repro: `[vector<i32, i32>] values{vector<i32>(1i32)}` —
  rejected at semantics with `vector requires exactly one template
  argument [PSC1005]`, before `classifyCollectionSpecialization` (which
  only runs off already-accepted binding facts) ever sees this binding.
- **R13/H4b** (`soa`-base, template-arg count != 1). Repro:
  `[soa<Particle, i32>] values{soa<Particle>()}` — rejected identically:
  `soa requires exactly one template argument [PSC1005]`.
- **R13/H5b** (`map`-base, template-arg count != 2). Repro: `[map<i32>]
  values{map<i32, i32>()}` — rejected identically: `map requires exactly
  two template arguments [PSC1005]`.
- **R13/H2's doubly-wrapped `Reference<Pointer<...>>` shape.** Repro:
  `useit([Reference<Pointer<vector<i32>>>] values) { ... }` — rejected at
  semantics with `unsupported reference target type:
  Pointer<vector<i32>> [PSC1005]`, i.e. `Reference<...>` only accepts a
  non-wrapper inner type; the doubly-wrapped shape never reaches binding
  publication at all.
- **R13/H2's doubly-wrapped `Pointer<Reference<...>>` shape.** Repro:
  `useit([Pointer<Reference<vector<i32>>>] values) { ... }` — rejected
  symmetrically: `unsupported pointer target type:
  Reference<vector<i32>> [PSC1005]`.
- **F3-N2** (monomorphization: an unbound `Name`-kind method-call
  receiver — one with no matching entry in `locals` — has no matching
  branch in the F3 receiver-type-inference cascade, so `typeName` stays
  empty and the call would fall to F5's `return false`). Repro:
  `return(bogus_receiver_name.count())` with no such binding declared —
  rejected at the **semantics** stage itself:
  `validateExprMethodCallTarget failed name=count ns= resolved=/count
  templateArgs=0 args=1 receiver=Name:bogus_receiver_name ns=
  [PSC1005]`, before monomorphization ever runs. Consistent with the
  broader pattern this document's fifth round already established:
  semantics validation gates receiver-shape sanity upstream of the later
  two stages for most malformed-receiver shapes.

Together these six confirm (three of the six being the H2 sub-shapes,
resolving the fifth round's own "no test found for a `Pointer<...>` wrap,
nor for a doubly-wrapped ... chain" note completely: the single-wrap case
is live-and-uncovered, both double-wrap cases are unreachable) — this
round's H2/H3b/H4b/H5b/H6 sweep fully closes out R13's entire
"return false"/"positive-shape" open-question set the fifth round's
"Still open" list named, except H2b (below).

### Inconclusive after reasonable effort (left `UNPINNED`, with what was
tried)

- **R13/H2b** (`Reference`/`Pointer`-base collection-specialization:
  `splitTopLevelTemplateArgs` fails or yields != 1 arg → `return false`).
  Attempting the obvious repro (`Reference<i32, i32>`, expecting a
  same-shape rejection to H3b/H4b/H5b's pattern) instead surfaced a
  wrinkle not previously documented anywhere in this table: PrimeStruct's
  `Reference<T, Capability>` is apparently a **legitimate**, real 2-arg
  binding shape (a capability spelling as the second argument — the repro
  failed with `unknown Reference capability: i32 (expected Read, Write,
  or ReadWrite) [PSC1005]`, not an arity-rejection message), distinct
  from H2b's "malformed wrapped-collection with the wrong arg count"
  framing. This means H2b's own guard condition ("!= 1 arg") may not be
  the operative gate for the common malformed case at all — a
  capability-bearing `Reference<vector<T>, Read>` might be the shape that
  actually reaches this branch, not a bare wrong-arity wrapper. Did not
  determine within this round's budget whether `Reference<vector<T>,
  Capability>` (a *valid* 2-arg reference-with-capability shape wrapping
  a collection) reaches H2's normal path, H2b's rejection, or a third,
  undocumented branch — left open with this concrete new lead for a
  future round instead of guessed at.
- **Row G's G3b** (`ir_lowerer`: `resolvedPath ==
  "/std/collections/soa/to_aos"` treated as a deliberate "no definition,
  not an error" sentinel inside the semantic-product-driven G3 cascade).
  Found a related but distinct existing unit test
  (`test_ir_pipeline_validation_ir_lowerer_setup_type_helper_resolves_method_definitions_from_receiver_targets.cpp`)
  that exercises `resolveMethodDefinitionFromReceiverTarget` (G8's own
  helper, a different, lower-level function in the same cascade) directly
  against both a bare `/to_aos` and the canonical
  `/std/collections/soa/to_aos` `defMap` entry — but this does not
  exercise G3b's specific condition, which requires
  `findSemanticProductMethodCallTarget` to have already resolved and
  returned exactly that literal path string as a semantic-product-stage
  fact (a different, earlier point in the cascade than G8's unit-level
  test reaches). Did not trace `findSemanticProductMethodCallTarget`'s
  own producer far enough this round to construct a `.prime` repro that
  reliably lands exactly on this sentinel value rather than some other
  resolved/empty path — left open, flagged for a future round with the
  above unit test noted as adjacent-but-not-equivalent coverage.

### Still open — not attempted this round

The remaining ~46 UNPINNED-tagged rows were not individually
cross-referenced this round. Grouped by document row category for
whoever picks this up next (largely the fifth round's own list, minus
the items this round resolved):

- **Row A**: the `isBuiltinOut` per-branch-asymmetry note (no dedicated
  row ID).
- **Row B**: `classifyExplicitVectorHelperReceiver`'s fixed-order-priority
  contract (the order itself, not its per-family consequences).
- **Row E**: E5, E6, E7's R10-lacks-D5-guard asymmetry, F6, F7, F9b, F10,
  R12's G3/G4/G5/G7/G8 sub-rows, R13's production-gate-piggyback-on-
  `binding_facts` design question, R14's Q2/Q3/Q3b/Q4/Q5/Q5b and its own
  Pass-2-redundancy question.
- **Row F**: F6 (wrapper-method-path), F7 (File-method dispatch details),
  F9/F13/F13b/F13c (primitive/collection-family no-definition fallbacks),
  F15, F16, F3-C3a/b/c/d (the struct-return-path and `return<T>`-
  annotation override-priority gaps).
- **Row G**: G1, G2, G3c-i through G3c-v, G3d's per-sub-guard detail,
  G9b through G9f, the CH-V4/CH-V5 and CH-V6/CH-V7/CH-V8 asymmetries in
  `resolveVectorHelperAliasName`, and `normalizeMapImportAliasPath`'s
  identity-function purpose.
- **New leads from this round's inconclusive rows**: H2b's
  `Reference<T, Capability>`-vs-wrapped-collection ambiguity (above), and
  G3b's `findSemanticProductMethodCallTarget` producer chain (above).

**Cumulative running total across all rounds so far.** Approximately 22
of the ~68 originally-tagged UNPINNED rows/sub-guards have now been
individually cross-referenced (round five's ~14 plus this round's 8),
resolving to some combination of confirmed-zero-coverage,
confirmed-latent-only, confirmed-dead-code (this round's new resolution
class — a stronger determination than either of the other two, since it
proves the branch cannot execute for *any* input, not just that no test
happens to reach it), or inconclusive-with-documented-attempts, leaving
roughly 46 still open for future rounds. Consistent with the fifth
round's own observation: rows this document already flagged as
suspicious (an explicit "asymmetry" or "not confirmed reachable" note)
continue to resolve to real findings rather than false alarms at a high
rate, and semantics-stage upstream validation continues to be the
dominant reason a documented lower-stage gap turns out to be
latent-only rather than live.

## Step 1b: diff-audit harness wired at resolveArgsPackElementMethodTarget, zero-divergence achieved (2026-09-08)

Following the Plan's own staged discipline: extended
`ReceiverElementFamilyClassifier` and wired the differential-audit harness
into exactly **one** call site this round -
`resolveArgsPackElementMethodTarget`
(`SemanticsValidatorMethodTargetArgsPackResolvers.cpp`), Row category A's
entry point and the most thoroughly characterized single function in the
Step 0 rule table. No other call site was touched.

**Classifier extension.** Added
`classifyReceiverElementFamilyJoint(ReceiverElementFamilyJointInput,
ReceiverElementFamilyPredicates)` alongside the existing (still-unwired)
Step 1a `classifyReceiverElementFamily`, in
`include/primec/support/ReceiverElementFamilyClassifier.h` /
`src/support/ReceiverElementFamilyClassifier.cpp`. It replicates R1-R7 of
Row category A's cascade exactly, resolving both quirks Step 1a's header
flagged as open:

- **FileError method-name gating (R2/R2b).** `FileError` only classifies
  as the `FileError` family when `normalizedMethodName` is one of `{why,
  is_eof, status, result}`; any other method name **falls through**
  (continues evaluation, does not reject) to the template-shape block and
  then the Primitive/StructOrUnknown fallback - reproduced by not
  returning early on a method-name mismatch, exactly mirroring
  production's control flow rather than special-casing it.
- **Template-shape gating (R3-R6b).** The `VectorLike`/`Soa`/`Buffer`/
  `KeyValue`/`File` checks only run when the caller reports
  `isTemplateShaped == true` (computed by the caller's own
  `splitTemplateTypeName`, not re-implemented in the classifier - its
  "matching `>` at the exact end of the string" requirement stays
  stage-owned). A bare non-template `"Buffer"` or `"File"` element type
  skips the whole block and falls to Primitive/StructOrUnknown, reproduced
  verbatim (`R6b`).
- A third, previously-undocumented but symmetric quirk fell out of
  extending the two above to `Buffer` and `File`'s own method-name gates
  (`R4b`: `Buffer<T>` with an unrecognized accessor method; the File-side
  equivalent of `R6b` for a template-shaped `File<T>` with an unrecognized
  handle method) - both are the same "commit only on method-name match,
  else fall through" shape as `R2b`, not a new independent finding, so
  folded into the same rows rather than given new IDs.
- A fourth, genuinely new observation surfaced while wiring the joint
  input: production's Primitive check (R7) runs against
  `normalizedElemBaseType` - the **non**-Reference/Pointer-unwrapped
  element type text - while every other branch (R1-R6) runs against
  `collectionElemType`, the **unwrapped** text. A `Reference<i32>`-typed
  args-pack element therefore never classifies as `Primitive` in
  production (the wrapped text `"Reference<i32>"` isn't in the primitive
  name set), even though its unwrapped type is a primitive - it falls to
  the struct-path fallback (R8) instead. The joint classifier takes two
  separate text inputs (`unwrappedElementType` for R1-R6,
  `rawElementBaseType` for R7) specifically to reproduce this asymmetry
  rather than silently "fixing" it - flagged in the header as a candidate
  fresh Step 0 finding for whoever next characterizes whether real
  `args<Reference<i32>>`-shaped elements are reachable in practice (not
  determined this round; out of scope for Step 1b, which only has to
  match production, not judge it).

Unit tests: `tests/unit/semantics/test_semantics_receiver_element_family_classifier.cpp`
gained 12 new `TEST_CASE`s (22 total in the suite, all passing) pinning
R1-R7 including both documented quirks, the R4b/File-mismatch
fallthroughs, R6b for both `Buffer` and `File`, and the R7
wrapped-vs-unwrapped asymmetry explicitly.

**Doctest pitfall found and fixed.** The first attempt named the new
logging helper `primec::toString(ReceiverElementFamily)`. Doctest's
`CHECK(... == ReceiverElementFamily::...)` stringification does an
ADL lookup for a function literally named `toString`, and colliding with
it broke compilation of every existing `==` comparison on this enum
(`invalid operands of types 'const char*' and 'const char*' to binary
'operator+'` inside `doctest.h`, confirmed via `git stash` that the file
built cleanly before this addition and broke immediately after adding
that one function). Renamed to `describeReceiverElementFamily` and the
build was clean again - documented in the header as a landmine for future
additions to this module rather than left to be rediscovered.

**Wiring mechanics.** `resolveArgsPackElementMethodTarget`'s own control
flow, argument types, and return values are **completely unmodified**.
The only additions: (1) a single cached `isReceiverTargetDiffAuditEnabled()`
check near the top: if unset, the block that would compute the classifier
verdict is skipped entirely (the `if (diffAuditEnabled) { ... }` body never
runs); (2) one `auditFamily(...)` call inserted immediately before each of
the function's existing `return` statements, which - when the env var is
set - compares the family that production branch represents against the
already-computed classifier verdict and logs a `[receiver-target-diff-audit]
MISMATCH ...` line to stderr (plus a debug-only `assert`, a no-op in this
Release build) on disagreement; when unset, `auditFamily` is a no-op
(single boolean check, immediate return). No existing line's return
expression, evaluation order relative to its own side effects, or the
values written to `resolvedOut`/`isBuiltinOut` changed.

**Zero-divergence proof.** Ran the full 3-suite battery
(`PrimeStruct_semantics_tests`, `PrimeStruct_backend_ir_tests`,
`PrimeStruct_compile_run_tests`) with `PRIMESTRUCT_RECEIVER_TARGET_DIFF_AUDIT=1`
set:

| suite | test cases | failed | assertions | failed | `[receiver-target-diff-audit] MISMATCH` lines |
|---|---|---|---|---|---|
| semantics | 2766 | 1 | 13341 | 2 | **0** |
| backend_ir | 1646 | 46 | 16428 | 137 | **0** |
| compile_run | 2679 | 5 | 15278 | 8 | **0** |

Zero mismatch lines across all three suites - every call this function
made while the harness was active agreed with the classifier's verdict.
Failure counts above (1/46/5, the same pre-existing baseline this session
independently re-confirmed byte-for-byte by test name before making any
change - see below) are unrelated pre-existing issues, not caused by this
round's work.

**Unchanged-default-behavior proof.** Ran the same battery with the env
var unset (default) and diffed the *set* of failing test-case names
against a freshly re-confirmed pre-change baseline (`git stash`, rebuild,
rerun, `git stash pop`, rebuild again) using `diff` on sorted name lists,
not just counts:

- `PrimeStruct_semantics_tests`: baseline and post-change failing-name
  sets both exactly `{"semantic product validates direct return
  method-like borrowed helper-return experimental soa reads"}` (the same
  pre-existing flake this document's Update sections already reference) -
  `diff` empty.
- `PrimeStruct_backend_ir_tests`: baseline and post-change failing-name
  sets both the same 46 names (the pre-existing baseline this document's
  2026-09-05/06 Update already re-confirmed as not a regression) - `diff`
  empty.
- `PrimeStruct_compile_run_tests`: baseline and post-change failing-name
  sets both the same 5 names (matches the 2026-09-06 Update's "dropped
  ... to 5 failures" state) - `diff` empty.

Total test-case counts are up by 12 in `semantics` (the new classifier
unit tests themselves) and unchanged in the other two suites, exactly as
expected for a purely-additive, env-gated change with no test additions
in those files.

**Conclusion.** Zero divergence achieved for this one call site on the
first attempt - no classifier iteration was needed; the two Step 1a quirks
plus the two new joint-extension findings (Buffer/File method-mismatch
fallthrough, the wrapped-vs-unwrapped Primitive-check asymmetry) fully
account for production's behavior at this call site. The harness stays
wired and env-gated off by default. No call site was switched to use the
classifier's verdict for real (Step 2, not attempted this round, per the
task's explicit scope).

## Step 1b slice 2: diff-audit harness wired at resolveMethodTarget's indexed-args-pack cascade, zero-divergence achieved (2026-09-08)

Continuing Step 1b's own staged plan (one more call site, same pattern,
never batching): wired the differential-audit harness into a second call
site this round, found by looking for the next well-scoped, single-
function target among Row category A/B/C's remaining call sites.

**Why this call site, not `classifyExplicitVectorHelperReceiver` or the
Row C "is this a map receiver" candidates.** Both were read in full this
round before choosing. `classifyExplicitVectorHelperReceiver` (Row B)
delegates to six large sub-resolvers (`resolveVectorTarget`,
`resolveSoaVectorTarget`, `resolveArrayTarget`, `resolveBorrowedVectorReceiver`,
...), each 40-150 lines with its own multi-branch cascades over binding
shape, call-expression inference, and SOA-conversion special-casing - a
sprawling, not-single-bounded-function shape unlike this task's own
"smaller, cleaner target" guidance. Row C's five independent "is this a
map receiver" implementations
(`isCanonicalKeyValueReceiver`/`isWrappedKeyValueReceiver`/their two nested
extractors/`resolveKeyValueTarget`) are boolean receiver-shape predicates
gated on binding/field/call-expression *shape*, not the
`(type, methodName, templateShape) -> family` decision shape this
classifier models - forcing them onto `classifyReceiverElementFamilyJoint`
would mean guessing at a mapping rather than reusing a genuinely identical
decision, the exact anti-pattern this document's Step 1a/1b sections
already warn against.

Instead, re-reading `SemanticsValidatorExprMethodTargetResolution.cpp`
(the file housing `resolveMethodTarget`, the ~1100-line top-level
dispatcher slice 1's own call site is invoked from) turned up an inline
block, lines ~1890-1947, handling the *indexed* args-pack-element method-
call shape (`pack[i].method()`, as opposed to slice 1's plain
`pack_elem.method()` shape) that is a near-verbatim second, independently-
coded copy of slice 1's own R1/R3-R7 cascade: string check, then a
template-shape-gated vector/array/soa -> Buffer -> key-value -> File
block, then a FileError check, then the primitive check, then the struct-
type-path fallback. This is exactly Row category A's own shape (the doc's
"resolveArgsPackElementMethodTarget and its siblings" framing already
anticipated a family of call sites like this), well-bounded (one ~60-line
`if` block inside a much larger function, touched without altering
anything else in that function), and reuses the *same* classifier
verdict-space this round already built - the ideal next slice.

**Classifier extension: none needed.** Auditing this block's exact inputs
found it needs no new branch, family, or predicate - `classifyReceiverElementFamilyJoint`
already covers it. Two structural differences from slice 1 were found and
resolved by understanding, not by adding code:

- **No wrapped-vs-unwrapped Primitive-check asymmetry.** Slice 1's R7
  quirk (documented above and in the header) exists because
  `resolveArgsPackElementMethodTarget` computes its Primitive-check text
  (`normalizedElemBaseType`) *before* unwrapping `Reference<T>`/`Pointer<T>`,
  while every other branch there runs on the *unwrapped* text. This call
  site's `accessElemType` is already passed through
  `unwrapReferencePointerTypeText` before either `normalizedElemType` or
  `normalizedElemBaseType` is computed (line ~1891), so both are the exact
  same already-unwrapped string here - there is no wrapped/unwrapped split
  to model. The wiring passes that single string as both
  `unwrappedElementType` and `rawElementBaseType`, which is exactly
  `jointInputFor`'s existing test-helper default (`rawBase` defaults to
  `unwrapped` when omitted) - confirmed by a new unit test contrasting the
  two call sites' conventions on the identical original element type (see
  below).
- **FileError check position.** This block checks FileError *after* the
  template-shape block (opposite of slice 1's R2-before-R3 ordering).
  Proven behavior-preserving, not merely assumed: a template-shaped type's
  parsed base name (`splitTemplateTypeName`'s output) can never equal the
  bare literal `"FileError"` - that would require unparsed text like
  `"FileError<...>"`, which neither call site's cascade, nor any test
  corpus grep this round, produces - so the two check orderings are
  mutually exclusive on every real input and the classifier's own fixed
  ordering (FileError checked before the template block) reproduces both
  call sites' verdicts identically. Documented in the header rather than
  re-derived as a new rule row, since it changes no observable behavior.

Both findings are recorded on `classifyReceiverElementFamilyJoint`'s own
header comment (not just here) so a future reader wiring a third call site
does not mistake either for an unmodeled divergence.

**Unit tests.** One new `TEST_CASE` added (23 total in the classifier's
test file, all passing) explicitly pinning slice 2's "both classifier
inputs receive the same already-unwrapped text" contract, contrasted
against slice 1's wrapped-raw-text convention on the identical original
element type (`Reference<i32>` unwrapped to `i32`) - slice 2's convention
classifies `Primitive`, slice 1's classifies `StructOrUnknown`, same
classifier, different verdict, purely from which text each call site's
caller supplies.

**Wiring mechanics.** Identical pattern to slice 1, applied to this block
only: `resolveMethodTarget`'s control flow, argument types, and return
values inside and outside this specific `if` block are completely
unmodified. Additions: a cached `isReceiverTargetDiffAuditEnabled()`
check plus classifier-verdict computation gated on it (skipped entirely
when unset), and one `auditFamily2(...)` call inserted immediately before
each of the block's existing `return` statements (String, VectorLike/Soa,
Buffer, KeyValue - covering both of its two return paths, File, FileError,
Primitive, and the struct-path-found case). The one point in this block
that does *not* return - the struct-path-fallback-empty case, which falls
through to further, unrelated fallback resolution elsewhere in the
enclosing function (`resolveStringTarget`, `resolveKeyValueValueType`,
...) rather than committing to an answer of its own - is deliberately left
unaudited: unlike slice 1's R9 (which does `return false` as
`resolveArgsPackElementMethodTarget`'s own terminal verdict), this
cascade never treats "no match" as its own final answer, so there is no
production decision at that point to compare against. Auditing there would
compare the classifier's opinion against a decision production simply
does not make at that spot.

**Zero-divergence proof.** Ran the full 3-suite battery with
`PRIMESTRUCT_RECEIVER_TARGET_DIFF_AUDIT=1` set (both slices wired and
active simultaneously - the harness's own design means each call site's
audit is independent, so this exercises both slice 1's and slice 2's
verdicts in the same runs):

| suite | test cases | failed | assertions | failed | `[receiver-target-diff-audit] MISMATCH` lines |
|---|---|---|---|---|---|
| semantics | 2767 | 1 | 13343 | 2 | **0** |
| backend_ir | 1646 | 46 | 16428 | 137 | **0** |
| compile_run | 2679 | 5 | 15294 | 8 | **0** |

(Test-case count is up by 1 from slice 1's own recorded 2766 - this
round's one new classifier unit test; failure counts (1/46/5) match the
pre-existing baseline this round independently re-confirmed fresh before
any change, below.)

**Unchanged-default-behavior proof.** Ran the same battery with the env
var unset and diffed the *set* of failing test-case names (not just
counts) against a freshly re-confirmed pre-change baseline - `git status`
confirmed a clean tree at `4c2cfdb` before starting, then the existing
`build-release` binaries (already built from that exact commit) were run
directly with `-r=console -d`, parsed for the `TEST CASE:` line
immediately preceding each `ERROR:`/`is NOT correct!` line per suite, to
get the baseline failing-name set - then the same procedure was repeated
after the change, with the env var unset:

- `PrimeStruct_semantics_tests`: baseline and post-change failing-name
  sets both exactly `{"semantic product validates direct return
  method-like borrowed helper-return experimental soa reads"}` (the same
  pre-existing flake this document's earlier Update sections reference) -
  diff empty. Test-case count up by 1 (2767 vs 2766), matching the one new
  unit test.
- `PrimeStruct_backend_ir_tests`: baseline and post-change failing-name
  sets both the same 46 names - diff empty, test-case count unchanged
  (1646).
- `PrimeStruct_compile_run_tests`: baseline and post-change failing-name
  sets both the same 5 names - diff empty, test-case count unchanged
  (2679). One incidental observation: the suite's own *total passed-
  assertion* count varied between two back-to-back post-change runs with
  identical code and environment (15294 in one run, 15278 in another) even
  though the failing-name set and per-test SUCCESS/ERROR line counts were
  byte-identical between them - a pre-existing run-to-run nondeterminism
  in this suite (not traced further this round; out of this task's scope),
  not a regression from this change. The *failed*-assertion count (8) and
  failing-test-name set stayed fixed across all runs regardless.

**Conclusion.** Zero divergence achieved for this second call site on the
first attempt, same as slice 1 - the classifier needed no new logic, only
a documented understanding of how its two existing inputs map onto this
call site's already-unwrapped text and reordered FileError check. The
harness stays wired and env-gated off by default at both slices. No call
site was switched to use the classifier's verdict for real (Step 2, not
attempted this round).

## Step 2: resolveArgsPackElementMethodTarget migrated to classifyReceiverElementFamilyJoint, zero-divergence achieved (2026-09-08)

Per the Plan's Step 2 - the actual payoff of this whole consolidation
effort - migrated one of the two Step 1b diff-audit-harnessed call sites
for real: `resolveArgsPackElementMethodTarget`
(`SemanticsValidatorMethodTargetArgsPackResolvers.cpp`), Row category A's
entry point. Chosen over the second harnessed site
(`resolveMethodTarget`'s indexed-args-pack cascade, slice 2) because it is
its own dedicated function with a clean, already-fully-characterized
boundary, rather than one `if` block embedded in a much larger dispatcher -
the cleaner first migration this round's task explicitly asked for.

**What changed.** The function's own ~70-line inline classification
cascade (R1: string check; R2/R2b: FileError method-name-gated check;
R3-R6b: template-shape-gated vector/array/soa -> Buffer -> key-value ->
File block; R7: primitive check; R8/R9: struct-type-path fallback) is
replaced by: (1) computing the same `ReceiverElementFamilyJointInput` the
Step 1b diff-audit harness was already building for observation (no new
computation - this promotes existing audit-only code into the primary
path), (2) one call to `classifyReceiverElementFamilyJoint`, (3) a
family-keyed `switch` whose body per family is *exactly* the same
downstream action the old cascade's matching branch ran - same call
targets, same helper calls (`preferredFileErrorHelperTarget`,
`preferredBufferMethodTarget`, `setPreferredKeyValueMethodTarget`,
`preferredFileHelperTarget`, `resolveMethodTargetStructTypePath`/
`resolveTypePath`), same `isBuiltinOut` assignments. The Step 1b
diff-audit-harness scaffolding (the cached `isReceiverTargetDiffAuditEnabled()`
check, the `auditFamily` lambda, the per-return `auditFamily(...)` calls)
is removed entirely from this call site - per this round's own task
framing, once a call site delegates to the classifier for real there is no
longer a second, independently-coded inline answer to diff it against;
retaining the harness there would silently compare the classifier's
verdict with itself, which proves nothing and would create a false sense
of ongoing verification. This supersedes, not just supplements, the Step
1b slice 1 harness at this one call site - the harness stays live and
meaningful at the still-unmigrated slice 2 call site.

**Two places downstream logic had to use the call site's own local
variables, not the classifier result's fields, to stay byte-faithful:**

- **Primitive branch (R7).** Production builds the resolved path from
  `normalizedElemBaseType` - the *non*-Reference/Pointer-unwrapped text,
  per the documented R7 wrapped-vs-unwrapped asymmetry (Step 1b's own
  finding). The classifier result's `normalizedElementBaseType` field is
  always derived from the classifier's `unwrappedElementType` input
  (see `classifyReceiverElementFamilyJoint`'s implementation - it sets
  that field unconditionally at the top, before any branch runs), so using
  it here would silently discard that asymmetry for a
  `Reference<i32>`/`Pointer<i32>`-typed args-pack element. The migrated
  code keeps its own local `normalizedElemBaseType` variable (computed
  once, before the classifier call, exactly as before) and builds the
  resolved path from that, not from `classified.normalizedElementBaseType`.
  Flagged in the header of the migrated function so a future reader does
  not "simplify" this into using the classifier's own field.
- **VectorLike/Soa branch (R3).** Here the classifier result's
  `collectionBaseName` field *is* safe to use directly - it is set from
  the caller-normalized `elemBase` inside the classifier's template-shaped
  block, the same value the old cascade itself used to build
  `"/" + elemBase + "/" + method"`. No asymmetry here; used as-is.

**Verification.** Fresh baseline first, not a trusted prior number: `git
stash` back to the unmodified tree at `1f03e3d` (confirmed via `git
status`), rebuilt `PrimeStruct_semantics_tests`/`PrimeStruct_backend_ir_tests`/
`PrimeStruct_compile_run_tests` clean, ran all three, and captured the
*set* of failing test-case names (not just counts) per suite:

| suite | test cases | failed | assertions | failed |
|---|---|---|---|---|
| semantics | 2767 | 1 | 13343 | 2 |
| backend_ir | 1646 | 46 | 16428 | 137 |
| compile_run | 2679 | 5 | 15278 | 8 |

(Exactly matching the numbers Step 1b slice 2 already recorded as the
pre-existing baseline - confirms no drift between sessions.) `git stash
pop` restored the migration, rebuilt clean (no new warnings), reran all
three suites:

| suite | test cases | failed | assertions | failed | failing-name diff vs baseline |
|---|---|---|---|---|---|
| semantics | 2767 | 1 | 13343 | 2 | **empty** |
| backend_ir | 1646 | 46 | 16428 | 137 | **empty** |
| compile_run | 2679 | 5 | 15278 | 8 | **empty** |

Every count identical, and `diff` on the sorted failing-test-case-name
list per suite came back empty for all three - the exact same test cases
fail before and after, byte-for-byte by name, not merely by count. No
divergence found; the migration is a pure refactor at this call site, as
Step 1b's own zero-divergence proof predicted it would be.

**Conclusion.** One fewer duplicated receiver-classification
implementation in the codebase: the ~70-line inline R1-R9 cascade plus the
~50-line diff-audit-harness scaffolding it carried (roughly 90 net lines
once the shared switch/classifier-call replacement is accounted for) are
gone from this call site, replaced by one classifier call and a
family-keyed dispatch over already-existing downstream helper calls. The
second harnessed call site (`resolveMethodTarget`'s indexed-args-pack
cascade, slice 2) was deliberately left as an observational harness this
round rather than rushed through as a second migration in the same pass -
per this round's own task guidance, one careful migration beats two
rushed ones. Every other Row A/B/C/D/E/F/G call site this document's Step
0 rule table catalogs still independently re-derives receiver family
membership.

## Step 2, second migration: resolveMethodTarget's indexed-args-pack cascade migrated to classifyReceiverElementFamilyJoint, zero-divergence achieved (2026-09-09)

Migrated the second (and, per Step 1b, last) already-harnessed call site:
`resolveMethodTarget`'s own inline indexed-args-pack-element cascade
(`SemanticsValidatorExprMethodTargetResolution.cpp`, the `pack[i].method()`
access shape - Step 1b slice 2's own call site), left deliberately
unmigrated in the first Step 2 round to avoid rushing two migrations in
one pass.

**What changed.** The block's own ~60-line inline cascade (string check;
template-shape-gated vector/array/soa -> Buffer -> key-value -> File
block; FileError check, positioned after the template block at this call
site; primitive check; struct-type-path fallback) is replaced by: (1)
computing the same `ReceiverElementFamilyJointInput` the Step 1b
diff-audit harness was already building for observation (no new
computation - promotes existing audit-only code into the primary path),
(2) one call to `classifyReceiverElementFamilyJoint`, (3) a family-keyed
`switch` whose body per family is *exactly* the same downstream action the
old cascade's matching branch ran - same call targets
(`setCollectionMethodTarget`, `preferredBufferMethodTarget`,
`setIndexedArgsPackKeyValueMethodTarget`/`setPreferredKeyValueMethodTarget`,
`preferredFileHelperTarget`, `preferredFileErrorHelperTarget`,
`resolveStructTypePath`/`resolveTypePath`), same `isBuiltinOut`
assignments. The Step 1b diff-audit-harness scaffolding at this call site
(the cached `isReceiverTargetDiffAuditEnabled()` check, the
`auditFamily2` lambda, the per-return `auditFamily2(...)` calls) is
removed entirely, for the same reason the first migration retired its own
harness: once a call site delegates to the classifier for real, diffing
the classifier's verdict against itself proves nothing.

**Both slice-2-specific input conventions preserved, not re-derived.**
Per this call site's own Step 1b writeup and the header comment on
`classifyReceiverElementFamilyJoint`:

- **No wrapped/unwrapped Primitive-check asymmetry.** This call site's
  `accessElemType` is already run through `unwrapReferencePointerTypeText`
  before either `normalizedElemType` or `normalizedElemBaseType` is
  computed, so both texts are already the same unwrapped string - unlike
  `resolveArgsPackElementMethodTarget`'s R7, which deliberately keeps a
  *non*-unwrapped `normalizedElemBaseType` for its Primitive branch. The
  migrated code passes `normalizedElemType` as `unwrappedElementType` and
  `normalizedElemBaseType` as `rawElementBaseType` (the same text, minus a
  leading `/`) - both classifier inputs and the Primitive branch's own
  `resolvedOut` construction all read from this call site's single
  unwrapped text, exactly as production always has here. No asymmetry to
  preserve at this call site, so - unlike the first migration - using
  `normalizedElemBaseType` directly for the Primitive branch needed no
  special-casing against the classifier's own `normalizedElementBaseType`
  field; they agree here by construction.
- **FileError-after-template-block ordering.** This block's FileError
  check textually followed the template-shape block (opposite of
  `resolveArgsPackElementMethodTarget`'s R2-before-R3 order), which Step
  1b slice 2 already proved behavior-preserving under the classifier's
  fixed FileError-before-template ordering: a template-shaped base name
  (`splitTemplateTypeName`'s output) can never equal the bare literal
  `"FileError"`, so the two orderings are mutually exclusive on every real
  input. The migration relies on that same proof rather than re-deriving
  it - no new evidence needed, since nothing about the classifier's
  internal ordering changed between the two migrations.
- **KeyValue branch's two-step dispatch.** Unlike
  `resolveArgsPackElementMethodTarget`'s KeyValue case (a single call to
  `setPreferredKeyValueMethodTarget`), this call site first tries
  `setIndexedArgsPackKeyValueMethodTarget` (the indexed-access-specific
  helper) and only falls back to `setPreferredKeyValueMethodTarget` if
  that returns false - a call-site-specific downstream detail, unrelated
  to family classification, preserved verbatim in the migrated `switch`'s
  `KeyValue` case exactly as the pre-migration `if` block had it.

**Verification.** Fresh baseline first, not a trusted number from either
prior report: confirmed a clean tree at `8bd5084` via `git status` (this
round's edit had not yet been made), rebuilt all three suites (already
up to date, no rebuild needed), and ran the full battery twice before
touching any code:

| suite | test cases | failed | assertions | failed |
|---|---|---|---|---|
| semantics | 2767 | 1 | 13343 | 2 |
| backend_ir | 1646 | 46 | 16428 | 137 |
| compile_run | 2679 | 5 | 15278 | 8 |

Both baseline runs produced byte-identical sorted failing-test-case-name
sets (`diff` empty) and identical assertion counts - no run-to-run
nondeterminism observed this round (unlike the mild passed-assertion-count
wobble Step 1b slice 2 noted for `compile_run` in an earlier session; not
reproduced here across either baseline or post-migration reruns). Every
count exactly matches both prior sessions' recorded numbers for this exact
baseline, confirming no drift.

Made the migration, rebuilt clean (no new warnings), and ran the full
battery twice more:

| suite | test cases | failed | assertions | failed | failing-name diff vs baseline |
|---|---|---|---|---|---|
| semantics | 2767 | 1 | 13343 | 2 | **empty** (both runs) |
| backend_ir | 1646 | 46 | 16428 | 137 | **empty** (both runs) |
| compile_run | 2679 | 5 | 15278 | 8 | **empty** (both runs) |

Every count identical across all four runs (2 baseline + 2 post-migration),
and `diff` on the sorted failing-test-case-name list per suite came back
empty in every comparison - baseline-run1 vs baseline-run2, baseline vs
post-migration-run1, baseline vs post-migration-run2, and post-migration-
run1 vs post-migration-run2. No divergence found; the migration is a pure
refactor at this call site, exactly as Step 1b slice 2's own zero-
divergence proof predicted.

**Conclusion.** Both Step 1b-harnessed call sites are now migrated onto
`classifyReceiverElementFamilyJoint` for real. This call site's own
~60-line inline cascade plus its diff-audit-harness scaffolding are gone,
replaced by one classifier call and a family-keyed dispatch over
already-existing downstream helper calls (net 35 lines removed per `git
diff --stat`, most of the removal being the retired harness). Every other
Row A/B/C/D/E/F/G call site this document's Step 0 rule table catalogs
still independently re-derives receiver family membership - in particular
`classifyExplicitVectorHelperReceiver` (Row B) and the five independent
"is this a map receiver" predicates (Row C) remain the wrong decision
shape for this classifier without further design work (per Step 1b slice
2's own reasoning for skipping them), and monomorphization's
`resolveMethodCallTemplateTarget` (Row F, 17 branches, already fully
characterized) is next in this document's own stage-by-stage migration
order - not attempted this round.

## Step 1b, monomorphization stage: diff-audit harness wired at resolveMethodCallTemplateTarget's F11 FileError sub-case, zero-divergence achieved (2026-09-09)

Following the exact same staged discipline the semantics-stage Step 1b
rounds used: start with observation on the smallest well-bounded slice,
not a migration, and not the whole 17-branch Row F cascade at once.

**Shape assessment (per this round's task instructions): does
`resolveMethodCallTemplateTarget`'s classification logic fit the existing
`classifyReceiverElementFamilyJoint`'s `(type, methodName, templateShape)
-> family` shape, or does it need extension?** Both, depending on which
part of the function is asked:

- Row F's **F3 receiver-type-inference sub-cascade** (the block that
  computes `typeName`/`wrappedReceiverTypeName`/`isBorrowedSoaReceiver`
  from the receiver *expression*'s kind - `Name`/`Literal`/`BoolLiteral`/
  `FloatLiteral`/`StringLiteral`/`Call`) is a genuinely different-shaped
  question than the classifier answers: it decides *what type the
  receiver has* (an expression-kind dispatch, several steps of which
  recurse into binding lookups, return-type annotations, and even a
  nested call to `resolveMethodCallTemplateTarget` itself), not *what
  family a known type/method pair belongs to*. Wiring an audit harness
  here would mean inventing a shared decision function for a problem the
  classifier was never designed to solve - out of scope for "wire the
  existing classifier," and exactly the kind of premature interface
  stretch this document's own Step 0 discipline warns against. Left
  unwired this round; F3's own branches stay fully characterized in the
  Step 0 Rule Table (F3-N1/N2, F3-L/B/Fl/S, F3-C1 through F3-C3d) but
  none delegate to anything.
- Row F's **F11 FileError sub-case** (`normalizedReceiverLeafName ==
  "FileError"` with a method name in `{why, is_eof, status, result}`,
  once `typeName` is already known) is a **direct, no-extension-needed
  match** for the classifier's existing FileError family branch (R2/R2b):
  same fixed 4-name method set, same "family membership + method-name
  gate" shape, no template-shape or struct-metadata predicate involved.
  This is the slice picked for this round's harness - see below.

No classifier interface extension was needed for this slice. (Whether F3
someday needs its own, differently-shaped shared decision function is an
open question for a future round, not resolved here - flagging it,
consistent with the task's own "characterize rather than guess"
instruction, rather than building one on spec.)

**Why this slice, not F3 or the rest of F0-F16.** The task's own guidance
named F3 as one candidate ("already the most thoroughly characterized
single piece") and a single well-isolated main-cascade branch as the
other. Per the shape assessment above, F3 turned out not to fit what this
classifier answers at all, so the well-isolated-branch option was taken
instead. F11's FileError sub-case is: (a) a single guard with a fixed,
already-known-and-tested 4-name method set, not entangled with any other
branch's control flow (F9's primitive check and F10's `args` check run
before it but can never themselves match a `"FileError"`-leafed
`typeName`, so reaching F11 is unconditional for that receiver shape);
(b) already flagged by name in this round's task instructions (the
"`FileError.eof()` reachability split" gap) as worth checking; and (c)
the one sub-case of F11 the classifier's existing family enum actually
models - `ImageError`/`ContainerError`/`GfxError` (F11's three siblings,
same shape, different static path domains) have no corresponding family
in `ReceiverElementFamily` and were deliberately left out of this slice's
scope, not merely overlooked.

**The `FileError.eof()` gap, re-examined at this call site.** F11's own
guard set is `{why, is_eof, status, result}` - four names, `eof` absent -
confirmed unchanged from the Step 0 Rule Table's F11/F11-eof rows. The
only path that reaches an `/eof`-style dispatch is F1 (a few dozen lines
earlier in the same function), and F1 fires on a **completely different
condition**: the receiver expression being a bare `Name` whose *literal
source spelling* is exactly `"FileError"` (i.e. `FileError.eof()` where
`FileError` is used as if it were the receiver's own identifier text, not
a variable bound to a `FileError`-typed value) - not on `typeName`
inference at all. This round's classifier-fit slice is scoped to F11
(the `typeName`-based leaf check) specifically, so F1's literal-spelling
special case is out of scope for this harness by design, not by
oversight - consistent with the Step 1a/1b precedent of scoping narrowly
and documenting what's deliberately excluded rather than folding
unrelated shapes into one slice. The gap itself (a bound `FileError`-typed
variable's `.eof()` call has no matching branch anywhere in this function
and falls through all the way to F16's generic fallback) is unchanged by
this round's work - still an open, undetermined-reachability finding per
the Step 0 table, not fixed or newly resolved here.

**The F12/F14 SOA "asymmetry" was checked against this round's own
instructions and correctly treated as closed, not reopened.** Per the
sixth Step 0 round's finding (cross-referenced, not re-derived this
round): F14 is unreachable dead code, not a live divergence - F12's guard
(`isTemplateMonomorphSoaReceiverType(normalizedTypeName)`) is a strict
subset of F14's guard over the identical six method-name pairs, and every
F12 branch unconditionally returns before execution could ever reach
F14's lines. No audit harness was wired for this pair; there is nothing
live to observe agreement or disagreement on.

**Wiring mechanics.** Added `#include
"primec/support/ReceiverElementFamilyClassifier.h"` (plus `<cassert>`/
`<iostream>`) to `TemplateMonomorphMethodTargets.cpp`. Inserted one
`if (isReceiverTargetDiffAuditEnabled() && normalizedReceiverLeafName ==
"FileError") { ... }` block immediately before F11's existing FileError
dispatch check - guarded so it only runs (a) when the env var is set and
(b) when the receiver's leaf type is literally `"FileError"`, so this
audit is a no-op for every other receiver shape regardless of the env
var. Inside the block: builds a `ReceiverElementFamilyJointInput` with
`unwrappedElementType`/`rawElementBaseType` both set to
`normalizedReceiverLeafName` (this call site has no separate
wrapped-vs-unwrapped text at this point in the function - `typeName` has
already gone through `normalizeCollectionReceiverTypeName` above, unlike
semantics-stage R7's asymmetry), `isTemplateShaped = false` (F11's guard
is a leaf-string compare, not a `splitTemplateTypeName` result), and
`normalizedMethodName = fileErrorMethodName` (the *already*
`normalizeFileErrorMethodName`-normalized value, i.e. `isEof` already
mapped to `is_eof` - mirroring how the already-migrated semantics call
sites pass their own pre-normalized method name into the classifier
rather than re-normalizing inside it). Compares the classifier's
`family == ReceiverElementFamily::FileError` verdict against production's
own `fileErrorMethodName ∈ {why, is_eof, status, result}` boolean, logs a
`[receiver-target-diff-audit] MISMATCH ...` line to stderr plus a
debug-only `assert` on disagreement. Production's existing FileError
dispatch check immediately below is completely unmodified - same
condition, same `selectStaticHelperOverloadPath` call, same return.

**Zero-divergence proof.** Fresh 3-suite baseline taken first (`git
status` confirmed clean before any change), then rebuilt with the harness
and reran all three suites with `PRIMESTRUCT_RECEIVER_TARGET_DIFF_AUDIT=1`:

| suite | test cases | failed | assertions | failed | `[receiver-target-diff-audit] MISMATCH` lines |
|---|---|---|---|---|---|
| semantics | 2767 | 1 | 13343 | 2 | **0** |
| backend_ir | 1646 | 46 | 16428 | 137 | **0** |
| compile_run | 2679 | 5 | 15278 | 8 | **0** |

Zero mismatch lines and identical test/assertion counts to the freshly
re-confirmed baseline in all three suites - this narrow FileError slice
needed no classifier iteration; F11's guard and the classifier's existing
R2 branch already agree on every call this build's corpus makes.

**Unchanged-default-behavior proof.** Ran the same battery with the env
var unset (default) and diffed the sorted failing-test-case-*name* sets
against the fresh baseline, not just counts:

- `PrimeStruct_semantics_tests`: baseline and post-change failing-name
  sets both exactly the one known pre-existing flake ("semantic product
  validates direct return method-like borrowed helper-return
  experimental soa reads") - `diff` empty.
- `PrimeStruct_backend_ir_tests`: baseline and post-change both the same
  46 pre-existing failing names - `diff` empty.
- `PrimeStruct_compile_run_tests`: baseline and post-change both the same
  5 pre-existing failing names - `diff` empty.

One operational note from this round, unrelated to the code change
itself: running two instances of `PrimeStruct_compile_run_tests`
concurrently (an artifact of this round's own retry sequencing, not
anything the harness does) caused genuine segfaults in the VM-backend
subprocess tests it spawns, with the doctest run never reaching its final
summary line - purely a resource-contention artifact of this
4-core/15GB sandbox, reproduced and then eliminated by re-running exactly
one instance at a time (confirmed via `ps`/`pgrep` before trusting a
result). Flagged here as a process-hygiene note for whoever runs this
suite next in a similar sandbox, not a finding about this document's
subject matter.

**Conclusion.** The smallest well-bounded slice into monomorphization is
now harnessed and green: F11's FileError sub-case, the one part of Row
F's 17-branch cascade already provably a no-extension-needed fit for the
existing classifier. F3 (the receiver-type-inference sub-cascade) is a
different-shaped problem and was deliberately left unwired, not migrated
onto anything. The rest of Row F - F1's literal-`Name`-spelled-`FileError`
special case, F2's indexed-args-pack-map shape, F6's wrapper-method-path
branch, F7's File-method dispatch, F8's removed-alias rejection, F9's
primitive dispatch (which, note for a future round, already calls the
exact same shared `semantics::isPrimitiveBindingTypeName` function the
semantics-stage call sites use - no divergence risk there by
construction, unlike everything else in this row), F10's `args`-leaf
special case, the ImageError/ContainerError/GfxError siblings of F11, and
F12/F13/F13b/F13c/F15/F16 - all remain unharnessed and unmigrated, per
this document's own "resist the temptation to cover the whole cascade"
staging discipline. Step 2 (real migration) for monomorphization was not
attempted this round, per the task's explicit scope.

## Step 2, monomorphization stage: resolveMethodCallTemplateTarget's F11 FileError sub-case migrated to classifyReceiverElementFamilyJoint, zero-divergence achieved (2026-09-09)

Migrated the Step 1b-harnessed monomorphization slice for real:
`resolveMethodCallTemplateTarget`'s F11 FileError sub-case
(`TemplateMonomorphMethodTargets.cpp`) - the first real migration in the
monomorphization stage, following the same "harness first, migrate once
proven zero-divergence" discipline the two semantics-stage migrations
used.

**What changed.** F11's own inline 4-name method-name gate (`why`,
`is_eof`, `status`, `result`) is replaced by: (1) building the same
`ReceiverElementFamilyJointInput` the Step 1b diff-audit harness was
already constructing for observation (no new computation - the
audit-only code is promoted into the primary path), (2) one call to
`classifyReceiverElementFamilyJoint`, (3) gating the existing dispatch
(`selectStaticHelperOverloadPath("/std/file/FileError/" +
fileErrorMethodName)`, same call, same return) on the classifier's
`family == ReceiverElementFamily::FileError` verdict instead of the
inline boolean. The resolved-path construction and the function's overall
control flow (falls through to the ImageError/ContainerError/GfxError
checks and eventually F16's generic fallback on a family mismatch,
exactly as F11's own fallthrough always did) are unchanged - only *how*
the family/gate decision is made moved, not what happens once it is
known, per this round's task scope. The Step 1b diff-audit-harness
scaffolding at this call site (the `isReceiverTargetDiffAuditEnabled()`
check, the `[receiver-target-diff-audit]` stderr line, the `assert`) is
removed entirely, along with the now-unused `<cassert>`/`<iostream>`
includes it needed - once a call site delegates to the classifier for
real, diffing the classifier's verdict against itself proves nothing, the
same reasoning both semantics-stage migrations used to retire their own
harnesses. No other part of Row F's still-unmigrated 17-branch cascade
(F1's literal-`Name`-spelled-`FileError` special case, F3's
receiver-type-inference sub-cascade, F9's already-shared-primitive-check,
F10's `args`-leaf case, the ImageError/ContainerError/GfxError siblings,
F12-F16, etc.) was touched.

**Verification.** Fresh baseline first, not a trusted prior number:
confirmed a clean tree at `538a1ec` via `git status` (`git stash`'d this
round's own edit), rebuilt all three suites clean, and ran the full
battery twice before touching any code:

| suite | test cases | failed | assertions | failed |
|---|---|---|---|---|
| semantics | 2767 | 1 | 13343 | 2 |
| backend_ir | 1646 | 46 | 16428 | 137 |
| compile_run | 2679 | 5 | 15278 | 8 |

Both baseline runs produced byte-identical sorted failing-test-case-name
sets (`diff` empty) - matching every prior session's recorded numbers for
this exact baseline, confirming no drift. `git stash pop` restored the
migration, rebuilt clean (no new warnings), and ran the full battery
twice more:

| suite | test cases | failed | assertions | failed | failing-name diff vs baseline |
|---|---|---|---|---|---|
| semantics | 2767 | 1 | 13343 | 2 | **empty** (both runs) |
| backend_ir | 1646 | 46 | 16428 | 137 | **empty** (both runs) |
| compile_run | 2679 | 5 | 15278 | 8 | **empty** (both runs) |

Every count identical across all four runs (2 baseline + 2
post-migration), and `diff` on the sorted failing-test-case-name list per
suite came back empty in every pairwise comparison - baseline-run1 vs
baseline-run2, baseline-run1 vs post-migration-run1, baseline-run1 vs
post-migration-run2, and post-migration-run1 vs post-migration-run2. No
divergence found; the migration is a pure refactor at this call site,
exactly as the Step 1b harness's own zero-divergence proof predicted.
Before trusting each `PrimeStruct_compile_run_tests` result, confirmed via
`pgrep`/`ps` that exactly one instance of the binary was running at a
time, per this document's own recorded segfault-artifact warning from the
prior round.

**Conclusion.** Monomorphization now has its first call site delegating
to `classifyReceiverElementFamilyJoint` for real, alongside the two
already-migrated semantics-stage call sites - proof this consolidation's
shared classifier generalizes across stages, not just within one. The
diff-audit-harness scaffolding this slice carried (the cached
`isReceiverTargetDiffAuditEnabled()` check, the comparison, the stderr
line, the assert, and the two includes it needed) is gone from this call
site. Every other Row A/B/C/D/E/F/G call site this document's Step 0 rule
table catalogs still independently re-derives receiver family
membership - in particular the rest of Row F's 17-branch cascade (16 of
17 branches, F11 now excepted) and all of `ir_lowerer` remain completely
untouched.

## Step 1b, monomorphization stage: diff-audit harness wired at resolveMethodCallTemplateTarget's F9 primitive slice, zero-divergence achieved (2026-09-09)

A second monomorphization diff-audit slice, following the exact
harness-first discipline the F11 slice above used - observation only,
no migration this round. Written by a prior session (uncommitted when its
container restarted), picked up and verified fresh in this round rather
than re-derived from scratch, since inspection found the diff sound and
complete.

**Which branch, and why it needed no leaf-name gate.** Per the Step 0
Rule Table (Row F): F9 is `isPrimitiveBindingTypeName(typeName)` ->
dispatch to `/<typeName>/<normalizedMethodName>` unconditionally, with no
method-name-leaf narrowing of its own (unlike F11, which only fires when
`normalizedReceiverLeafName == "FileError"`). Every call reaching this
point in the cascade is a candidate for F9, so the harness is wired
unconditionally (gated only on `isReceiverTargetDiffAuditEnabled()`),
immediately before production's own `isPrimitiveBindingTypeName(typeName)`
check - not narrowed to a leaf-name match the way F11's was, because F9
itself has no comparable narrowing to mirror.

**Wiring mechanics.** Same pattern as the F11 slice: builds a
`ReceiverElementFamilyJointInput` with `unwrappedElementType` and
`rawElementBaseType` both set to `typeName` (F9, like F11, has no
separate wrapped/unwrapped text at this point - `typeName` has already
gone through `normalizeCollectionReceiverTypeName` above), `isTemplateShaped
= false` (so the classifier's Soa/KeyValue predicate block, which is
template-shape-gated, is unreachable from this audit - the passed-in
`ReceiverElementFamilyPredicates{}` is a deliberately-inert default),
and `normalizedMethodName` passed through as-is. Compares the classifier's
verdict against `isPrimitiveBindingTypeName(typeName)`, logs a
`[receiver-target-diff-audit] MISMATCH ... (F9 primitive slice)` line to
stderr plus a debug-only `assert` on disagreement, then falls through to
production's own unmodified `isPrimitiveBindingTypeName` dispatch. Purely
observational - the audit block's result never feeds back into control
flow.

**The String/Primitive equivalence, verified.** The diff's own inline
comments claim: the classifier's separate String (R1) and Primitive (R7)
verdicts should *both* be treated as "production dispatches primitive-like"
for this comparison, because production's `isPrimitiveBindingTypeName`
folds `string` in with `i32`/`bool`/etc into the same bucket, and both
route through the identical `/<typeName>/<method>` path formula - so the
comparison uses `classifierFamily == Primitive || classifierFamily ==
String` as its "matches production" predicate, not `== Primitive` alone.
Checked this claim two ways rather than assuming it: (1) read
`isPrimitiveBindingTypeName`'s own definition and confirmed it does
include `"string"` in its name set, alongside the numeric/bool primitive
names - so production genuinely treats `string` as F9-dispatchable, not
merely by coincidence of this one call site; (2) ran the full audited
battery and confirmed **zero** MISMATCH lines were produced at all,
across all three suites' entire corpus of receiver types including every
`string`-typed call this corpus exercises - if the equivalence claim were
wrong, a bare `classifierFamily == Primitive` comparison (String excluded)
would have produced a MISMATCH on every `string`-receiver primitive-style
call in the corpus, and it did not, corroborating the claim rather than
merely resting on the comment's say-so. The equivalence holds as
documented; no classifier bug found.

**Zero-divergence proof.** Fresh 3-suite baseline taken first via `git
stash` back to a clean `5bcd79f` tree (confirmed via `git status`),
rebuilt, ran the full battery once. `git stash pop` restored the F9
harness, rebuilt clean, and ran the full battery with
`PRIMESTRUCT_RECEIVER_TARGET_DIFF_AUDIT=1`:

| suite | test cases | failed | assertions | failed | `[receiver-target-diff-audit] MISMATCH` lines |
|---|---|---|---|---|---|
| semantics | 2767 | 1 | 13343 | 2 | **0** |
| backend_ir | 1646 | 46 | 16428 | 137 | **0** |
| compile_run | 2679 | 5 | 15278 | 8 | **0** |

Identical test/assertion counts to the freshly re-confirmed baseline in
all three suites, and zero mismatch lines - this F9 slice needed no
classifier iteration either; production's `isPrimitiveBindingTypeName`
and the classifier's Primitive/String verdicts already agree on every
call this build's corpus makes, once the documented String/Primitive
equivalence is applied.

**Unchanged-default-behavior proof.** Ran the same battery with the env
var unset (default), twice, and diffed the sorted failing-test-case-*name*
sets against the fresh baseline - not just counts:

- `PrimeStruct_semantics_tests`: baseline and both post-change runs all
  exactly the one known pre-existing flake ("semantic product validates
  direct return method-like borrowed helper-return experimental soa
  reads") - `diff` empty in every pairwise comparison.
- `PrimeStruct_backend_ir_tests`: baseline and both post-change runs all
  the same 46 pre-existing failing names - `diff` empty.
- `PrimeStruct_compile_run_tests`: baseline and both post-change runs all
  the same 5 pre-existing failing names ("C++ emitter runs canonical map
  reference string access", "map wildcard import rejects stdlib-owned
  surface in C++ emitter", "runs collection literals with map at in C++
  emitter", "runs vm canonical map reference string access with imported
  canonical helpers", "runs vm shared stdlib map conformance harness") -
  `diff` empty in every pairwise comparison (baseline vs run1, baseline
  vs run2, run1 vs run2).

Before trusting each `PrimeStruct_compile_run_tests` result, confirmed
via `ps`/`pgrep` that exactly one instance of the binary was running at a
time, per this document's own recorded segfault-artifact warning from the
F11 round - no concurrent-run artifact seen this round.

**Conclusion.** F9's primitive-family slice is now harnessed and green,
purely observationally - no behavior change, no classifier iteration
needed, and the diff's own documented String/Primitive equivalence claim
checked out rather than assumed. Per the established one-slice-at-a-time
discipline, F9 is **not** migrated for real this round - that is a
separate future Step 2 round, matching how F11's harness (Step 1b) and
F11's real migration (Step 2) were deliberately kept as two separate
rounds. The rest of Row F's cascade - F1, F2, F3, F6, F7, F8, F10, the
ImageError/ContainerError/GfxError siblings of F11, F12-F16 - remains
unharnessed and unmigrated.

## Step 2, monomorphization stage: resolveMethodCallTemplateTarget's F9 primitive slice migrated to classifyReceiverElementFamilyJoint, zero-divergence achieved (2026-09-09)

Migrated the Step 1b-harnessed F9 slice for real, the second real
migration in the monomorphization stage (after F11), same
harness-first/migrate-once-proven discipline.

**What changed.** F9's own inline, unconditional
`isPrimitiveBindingTypeName(typeName)` gate is replaced by: (1) building
the same `ReceiverElementFamilyJointInput` the Step 1b diff-audit harness
was already constructing for observation (no new computation - the
audit-only code is promoted into the primary path), (2) one call to
`classifyReceiverElementFamilyJoint`, (3) gating the existing dispatch
(`selectHelperOverloadPath(expr, "/" + normalizeBindingTypeName(typeName)
+ "/" + normalizedMethodName, ctx)`, same call, same return) on the
classifier's family verdict being `Primitive` **or** `String` - per the
harness's own proven finding that production's `isPrimitiveBindingTypeName`
folds `string` into the same dispatch bucket as `i32`/`bool`/etc, both
routing through the identical path formula. `normalizeBindingTypeName` is
still called on the original `typeName` text below, exactly as before -
the classifier's verdict decides only whether this branch fires, never
what string it constructs; deliberately double-checked to avoid the
category of mistake where a classifier-derived value gets substituted for
`typeName` in path construction. The Step 1b diff-audit-harness
scaffolding at this call site (the `isReceiverTargetDiffAuditEnabled()`
check, the comparison, the `[receiver-target-diff-audit]` stderr line, the
`assert`) is removed entirely, along with the now-unused
`<cassert>`/`<iostream>` includes (confirmed unused file-wide first - no
other `assert`/`std::cerr`/`isPrimitiveBindingTypeName(` call remained in
this file after the removal). No other part of Row F's still-unmigrated
cascade (F0-F8 minus F9, F10, F12-F16, the ImageError/ContainerError/
GfxError siblings of F11) was touched.

**Verification.** Fresh baseline first, not a trusted prior number:
`git stash`'d this round's own edit back to a clean `4205980` tree
(confirmed via `git status`), rebuilt all three suites clean, and ran the
full battery once:

| suite | test cases | failed |
|---|---|---|
| semantics | 2767 | 1 |
| backend_ir | 1646 | 46 |
| compile_run | 2679 | 5 |

Identical to every prior session's recorded numbers for this exact
baseline - no drift. `git stash pop` restored the migration, rebuilt
clean, and ran the full battery twice more (plus a third, final run after
removing the now-dead `<cassert>`/`<iostream>` includes, to confirm that
cleanup itself changed nothing):

| suite | test cases | failed | failing-name diff vs baseline |
|---|---|---|---|
| semantics | 2767 | 1 | **empty** (all three post-migration runs) |
| backend_ir | 1646 | 46 | **empty** (all three post-migration runs) |
| compile_run | 2679 | 5 | **empty** (all three post-migration runs) |

Every failing-test-case count identical across all four runs (1 baseline +
3 post-migration), and `diff` on the sorted failing-test-case-*name* list
per suite came back empty in every pairwise comparison checked (baseline
vs each post-migration run, and each post-migration run against the
others). `PrimeStruct_compile_run_tests`' own assertion-total count
fluctuated by 16 (15278 vs 15294) on one of the four runs with the exact
same 5 failing names and 8 failed assertions both times - consistent with
this document's own previously-recorded environment-noise class, not a
real divergence, since the failing-name set (the criterion this
verification discipline is built around) never moved. Before trusting
each `PrimeStruct_compile_run_tests` result, confirmed via `pgrep` that
exactly one instance of the binary was running at a time, per this
document's own recorded segfault-artifact warning.

**Conclusion.** F9 is now migrated for real, alongside F11 - monomorphization
now has two call sites delegating to `classifyReceiverElementFamilyJoint`.
No diff-audit-harness scaffolding remains anywhere in
`TemplateMonomorphMethodTargets.cpp` at this point (both F9's and F11's are
now gone, their code fully promoted into production dispatch). Every other
Row A/B/C/D/E/F/G call site this document's Step 0 rule table catalogs
still independently re-derives receiver family membership - in particular
15 of Row F's 17 branches (F0-F8 minus F9, F10, F12-F16) and all of
`ir_lowerer` remain completely untouched.

## Step 1b, monomorphization stage: third diff-audit harness at F13/F13b/F13c collection-family slice, zero-divergence achieved (2026-09-09)

Wired a third Step 1b diff-audit harness in the monomorphization stage,
same discipline as F11's and F9's own harness rounds: observe only, do
not migrate this round.

**Branch chosen and why.** Row F's `isCollectionFamilyReceiver` membership
test (`TemplateMonomorphMethodTargets.cpp`, feeding F13/F13b/F13c: "no
source definition at `resolvedType`, dispatch generically when `typeName`
is `array`/`vector`/`map`/soa-family; else `string`-dispatch (F13b); else
reject (F13c)"). Considered and rejected two other candidates named in
this round's own steering: F10 (the bare-`args`-leaf-name branch) has no
matching family in `ReceiverElementFamily` at all - "args" is not a type
classification the shared enum models, so wiring it here would mean
inventing a new family rather than reusing an existing one, a worse fit
than F13; F12/F14 were left alone entirely per this round's explicit
instruction not to touch F14-adjacent code (F14 is known dead code, not
yet deleted). F13 was the clean fit: a plain family-membership test with
no method-name gating, mapping directly onto the classifier's
VectorLike/Soa/KeyValue verdicts.

**Wiring.** By the point in the cascade where `isCollectionFamilyReceiver`
is computed, `typeName` has already gone through
`normalizeCollectionReceiverTypeName` (same as the F9/F11 slices' own
prior notes) - it is already a bare base name (`"array"`, `"vector"`,
`"map"`, or a soa-family name) with no generic-argument text left for
`splitTemplateTypeName` to parse. The harness therefore feeds the
classifier's `isTemplateShaped`/`templateShapedBaseName` inputs that
already-known base name directly (`isTemplateShaped=true`,
`templateShapedBaseName=typeName`, and `unwrappedElementType=
rawElementBaseType=typeName` since there is no separate wrapped/unwrapped
text at this point either), mirroring F9's own "hand the classifier the
already-known answer instead of re-deriving a parse with nothing left to
do" approach for its `isTemplateShaped=false` case. The classifier's
`isKeyValueSurfaceTypeName` predicate is supplied as a literal `== "map"`
lambda - deliberately mirroring this *exact* production guard's own
literal check (production's `typeName == "map"`, not any real
struct-metadata-backed key-value surface lookup), since this audit's job
is proving this particular guard's disposition, not exercising the
classifier's more general key-value path. The `isInternalSoaCollectionTypeName`
predicate is `isTemplateMonomorphSoaReceiverType`, the same function
production itself calls. Purely observational: gated on
`isReceiverTargetDiffAuditEnabled()`, computes both
`isCollectionFamilyReceiver` (production) and
`classifierFamily ∈ {VectorLike, Soa, KeyValue}` (classifier), logs a
`[receiver-target-diff-audit] MISMATCH` line and asserts on disagreement,
never substitutes for `isCollectionFamilyReceiver` itself. Zero-cost (one
cached `getenv`) when the env var is unset - production's own
`isCollectionFamilyReceiver` value and its two use sites (the import-alias
substitution guard and the F13/F13b/F13c dispatch itself) are untouched.

**Verification.** Fresh baseline first, not a trusted prior number:
`git stash`'d this round's own edit back to the clean `2f2db975c` tree
(confirmed via `git status`), rebuilt all three suites clean, and ran the
full battery once with the env var unset:

| suite | test cases | failed |
|---|---|---|
| semantics | 2767 | 1 |
| backend_ir | 1646 | 46 |
| compile_run | 2679 | 5 |

Identical to every prior session's recorded numbers for this exact
baseline - no drift. `git stash pop` restored the harness, rebuilt clean,
then:

- Ran all three suites once with `PRIMESTRUCT_RECEIVER_TARGET_DIFF_AUDIT=1`
  set: **zero** `MISMATCH` lines logged in any suite, no assertion fired
  (no crash/abort), and the sorted failing-test-case-*name* set was
  byte-identical to the baseline in all three suites (`diff` empty).
- Ran all three suites twice more each with the env var unset (six runs
  total): every run's sorted failing-test-case-*name* set was
  byte-identical to the baseline (`diff` empty in all eighteen
  baseline-vs-run pairwise comparisons across the three suites).

| suite | env-set run: MISMATCH count | env-set run: names vs baseline | 2x env-unset reruns: names vs baseline |
|---|---|---|---|
| semantics | 0 | identical | identical (both reruns) |
| backend_ir | 0 | identical | identical (both reruns) |
| compile_run | 0 | identical | identical (both reruns) |

Before trusting each `PrimeStruct_compile_run_tests` result, confirmed via
`pgrep -af PrimeStruct_compile_run_tests` that no second instance of the
binary was concurrently running (only this session's own polling-loop
shell wrappers matched the grep pattern in their command text, not a
second live instance of the test binary), per this document's own
recorded segfault-artifact warning.

**Conclusion.** Zero-divergence proven for the F13/F13b/F13c
collection-family slice; production behavior is unchanged this round (the
harness is purely observational, as required). This is the third
diff-audit harness wired in the monomorphization stage, after F11's and
F9's own (both since migrated for real). F13/F13b/F13c's own real
migration landed the same day, immediately following this harness round -
see "Step 2, monomorphization stage: resolveMethodCallTemplateTarget's
F13/F13b/F13c collection-family slice migrated to
classifyReceiverElementFamilyJoint, zero-divergence achieved (2026-09-09)"
below. Remaining scope in Row F: F0-F8 (minus F9/F11), F10, F12/F14-F16
(14 of 17 branches), plus all of Row B/C/G and all of `ir_lowerer`.

## Step 2, monomorphization stage: resolveMethodCallTemplateTarget's F13/F13b/F13c collection-family slice migrated to classifyReceiverElementFamilyJoint, zero-divergence achieved (2026-09-09)

Migrated the Step 1b-harnessed F13/F13b/F13c slice for real, the third
real migration in the monomorphization stage (after F11 and F9), same
harness-first/migrate-once-proven discipline.

**What changed.** The inline `isCollectionFamilyReceiver` literal-set
check (`typeName == "array" || typeName == "vector" || typeName == "map"
|| isTemplateMonomorphSoaReceiverType(typeName)`) is replaced by: (1)
building the same `ReceiverElementFamilyJointInput` the Step 1b diff-audit
harness was already constructing for observation (no new computation -
the audit-only code is promoted into the primary path, with
`isTemplateShaped=true`/`templateShapedBaseName=typeName` since `typeName`
has already gone through `normalizeCollectionReceiverTypeName` above and
is already a bare base name with nothing left for
`splitTemplateTypeName` to parse - same "hand the classifier the
already-known answer" approach F9 and F11 both used), (2) one call to
`classifyReceiverElementFamilyJoint`, (3) computing
`isCollectionFamilyReceiver` as the classifier's family verdict being
`VectorLike`, `Soa`, or `KeyValue` (the harness's own proven
membership-test equivalence). Both use sites of
`isCollectionFamilyReceiver` unchanged downstream - the import-alias
substitution guard (only applies when the receiver is *not* a collection
family and no source definition exists) and the F13/F13b/F13c dispatch
itself (F13: generic `/<typeName>/<method>` through
`preferVectorStdlibHelperPath` + `selectHelperOverloadPath`; F13b: string
fallback to `/string/<method>`; F13c: `return false` rejection) - are
byte-identical to what they always did; only the *classification* moved.
The Step 1b diff-audit-harness scaffolding at this call site (the
`isReceiverTargetDiffAuditEnabled()` check, the comparison, the
`[receiver-target-diff-audit] MISMATCH` stderr line, the `assert`) is
removed entirely. Confirmed no other `assert(`/`std::cerr`/
`isReceiverTargetDiffAuditEnabled`/`describeReceiverElementFamily` use
remained anywhere else in the file, so the now-unused
`<cassert>`/`<iostream>` includes were removed too (net -72/+23 lines in
`TemplateMonomorphMethodTargets.cpp`). No other part of Row F's still-
unmigrated cascade (F0-F8 minus F9/F11, F10, F12, F14-F16) was touched.

**Verification.** Fresh baseline first, not a trusted prior number:
`git stash`'d this round's own edit back to the clean `f780157d7` tree
(confirmed via `git status`), rebuilt all three suites clean, and ran the
full battery once:

| suite | test cases | failed | assertions | failed assertions |
|---|---|---|---|---|
| semantics | 2767 | 1 | 13343 | 2 |
| backend_ir | 1646 | 46 | 16428 | 137 |
| compile_run | 2679 | 5 | 15278 | 8 |

Identical to every prior session's recorded numbers for this exact
baseline - no drift. `git stash pop` restored the migration, rebuilt
clean, and ran the full battery three more times:

| suite | test cases | failed | assertions | failed assertions | failing-name diff vs baseline |
|---|---|---|---|---|---|
| semantics | 2767 | 1 | 13343 | 2 | **empty** (all three runs) |
| backend_ir | 1646 | 46 | 16428 | 137 | **empty** (all three runs) |
| compile_run | 2679 | 5 | 15278 | 8 | **empty** (all three runs) |

Every count identical across all four runs (1 baseline + 3
post-migration), and `diff` on the sorted failing-test-case-*name* list
per suite came back empty in every pairwise comparison checked (baseline
vs. each post-migration run, and each post-migration run against the
others - 9 pairwise comparisons per suite, 27 total, all empty).

One `compile_run` run showed a `terminate called after throwing an
instance of 'std::bad_alloc'` / `Aborted` line mid-log, and a separate
`compile_run` attempt stalled (near-zero CPU for several minutes) and was
killed and rerun cleanly. Neither was a divergence: both crash/hang
symptoms are inside the *outer* test binary's own subprocess-spawning
test cases, not the doctest binary itself - confirmed by (a) the
`bad_alloc`/`Aborted` run's own final `[doctest] test cases: 2679 | 2674
passed | 5 failed` summary line being present and identical to baseline
once fully read (the abort text is from one of the 5 already-known
baseline failures, "runs vm shared stdlib map conformance harness", which
`test_compile_run_map_conformance_expectations.h` expects a *specific*
non-zero exit code from a spawned compiler/VM child process and asserts
on the mismatch - a `134`/SIGABRT child exit is exactly the kind of thing
that assertion is built to catch, not the harness process itself dying),
and (b) the stalled run being unambiguously incomplete (no summary line
at all, still writing new log lines slowly when checked, correctly
identified as a hang rather than a false completion and rerun to a clean
finish rather than trusted). Before trusting each `compile_run` result,
confirmed via `pgrep -af PrimeStruct_compile_run_tests` that exactly one
instance of the binary was running (or that none was, before starting a
fresh one) at a time, per this document's own recorded segfault-artifact
warning.

**Conclusion.** F13/F13b/F13c is now migrated for real, alongside F11 and
F9 - monomorphization now has three call sites delegating to
`classifyReceiverElementFamilyJoint`. No diff-audit-harness scaffolding
remains anywhere in `TemplateMonomorphMethodTargets.cpp` at this point
(F9's, F11's, and F13/F13b/F13c's are all now gone, their code fully
promoted into production dispatch). Every other Row A/B/C/D/E/F/G call
site this document's Step 0 rule table catalogs still independently
re-derives receiver family membership - in particular 14 of Row F's 17
branches (F0-F8 minus F9/F11, F10, F12, F14-F16) and all of `ir_lowerer`
remain completely untouched.

## Step 1b, monomorphization stage: fourth diff-audit harness at F7 File-family slice, zero-divergence achieved (2026-09-09)

Wired a fourth monomorphization diff-audit harness (observational only, no
behavior change), this time at `resolveMethodCallTemplateTarget`'s F7 slice
- the File-family dispatch, gated on `(typeName == "File" ||
normalizedReceiverLeafName == "File") && isFileMethodName(normalizedMethodName)`.

**Why this slice.** Every remaining Row F branch not yet touched
(F0-F8 minus F9/F11, F10, F12, F14-F16) was assessed against the classifier's
`(type, methodName, templateShape) -> family` interface before picking one:

- **F0/F5** are trivial cascade guards (`!isMethodCall`/empty
  args/name; `typeName.empty()`) with no receiver-type classification at
  all - nothing for a family classifier to decide.
- **F1** classifies the receiver *expression's own literal spelling*
  ("is this identifier text literally `FileError`"), not a resolved
  type - a fundamentally different question than the classifier answers.
  It also uses a 5-name method set including `eof`, which the classifier's
  fixed FileError set (4 names, no `eof`) does not match - a real,
  pre-existing quirk this document's own Step 0 table already flagged
  (see F1's row above), not something to force-fit onto the classifier by
  guessing which behavior is "correct."
- **F2** (`resolveIndexedArgsPackMapMethodTarget`) and **F6** (the
  wrapper Reference/Pointer method-path branch) both need broader
  per-call context beyond a `(type, methodName)` pair - `locals` lookups
  and args-pack-map shape detection for F2, `hasTemplatedDefinitionFamilyPath`/
  `hasDefinitionFamilyPath` existence checks for F6 - neither fits the
  classifier's pure-function interface without a much larger extension
  than this round's "smallest extension" guidance calls for.
- **F3** (the receiver-type-inference sub-cascade itself) was already
  assessed and explicitly rejected in the classifier header's own Step 1b
  comment: it answers "what type does this receiver expression have," not
  "what family does an already-known type/method pair belong to" - the
  wrong shape entirely, not merely a larger version of the right shape.
- **F8** (`isExplicitRemovedCollectionMethodAlias`) already delegates to a
  different, purpose-built classifier
  (`CollectionSpellingClassifier`/`CompatPathResolutionConsolidation.md`'s
  own solved problem) for a genuinely different question - compat-spelling
  rejection, not type-family membership. Out of this document's own
  stated scope (see "Non-Goals").
- **F10** dispatches only 3 hardcoded method names
  (`count`/`at`/`at_unsafe`) to the `array` family unconditionally when
  the leaf is `"args"` - a narrower, differently-shaped rule than the
  classifier's VectorLike family, which (once the family matches) accepts
  *any* method name unconditionally. Force-fitting F10 onto the
  classifier would either silently widen the accepted method set or
  require a new, F10-specific narrowing parameter - a real interface
  extension, not the "smallest extension" this slice needed.
- **F12** needs `isBorrowedSoaReceiver` state (to pick the `_ref`-suffixed
  wrapper method name) that the `(type, methodName)` classifier interface
  has no slot for at all - a genuine third input dimension, not covered
  by the existing joint-input struct.
- **F14** is confirmed dead code by this document's own prior
  finding ("Headline finding: Row F's F12/F14 SOA 'asymmetry' is not an
  asymmetry - F14 is dead code" above); left untouched per this round's
  explicit instruction not to touch F14-adjacent code.

**F7 was the clean fit, needing zero interface extension.** The
classifier's existing File family check (inside its template-shape-gated
block) already calls `isFileHandleMethodName`
(`src/support/ReceiverElementFamilyClassifier.cpp`), which mirrors this
file's own `isFileMethodName` lambda (line ~145) name-for-name - both are
the identical 11-name set (`write`/`writeLine`/`write_line`/`writeByte`/
`write_byte`/`readByte`/`read_byte`/`writeBytes`/`write_bytes`/`flush`/
`close`), verified by direct side-by-side comparison, not assumed. And
- exactly like the already-migrated F13/F13b/F13c slice - by the point F7
runs, `typeName` has already gone through `normalizeCollectionReceiverTypeName`
above and is already reduced to a bare leaf name with nothing left for
`splitTemplateTypeName` to parse, so this harness feeds the classifier's
`isTemplateShaped`/`templateShapedBaseName` inputs the already-known leaf
(`normalizedReceiverLeafName`) directly - the same "hand over the
pre-parsed base" approach F9/F11/F13 all used. Production's guard is an OR
of `typeName` itself and the leaf; in every case actually observed
(`typeName == "File"` implies the leaf is also `"File"`, since no internal
`/` is present), the two conditions are equivalent - not proven
unreachable in the general case, just not observed, and the harness
compares against production's actual OR'd condition (not just the leaf)
so any such divergence would still surface as a MISMATCH rather than
being silently assumed away.

**Wiring mechanics.** Same pattern as the F9/F11/F13 harnesses: an
`isReceiverTargetDiffAuditEnabled()`-gated block placed immediately before
F7's existing `if` (unchanged), building a `ReceiverElementFamilyJointInput`
from the already-computed `normalizedReceiverLeafName`/`normalizedMethodName`,
calling `classifyReceiverElementFamilyJoint` with a default-constructed
(never-matches) `ReceiverElementFamilyPredicates` (Soa/KeyValue predicates
are structurally unreachable for a `"File"` leaf, since the classifier
checks VectorLike, then Soa, then Buffer, then KeyValue, then File in that
fixed order - none of the earlier checks can match `"File"`), comparing the
classifier's `File`-vs-not verdict against production's own boolean, and
logging/asserting on mismatch. `#include <cassert>`/`<iostream>` were
re-added (removed when F13's harness was retired into real migration).
Purely observational - the harness computes an answer and compares it;
production's own `if` immediately below is untouched, byte-identical to
before this round.

**Verification.** Fresh baseline first, not a trusted prior number:
`git stash`'d this round's own edit back to the clean `0713e5bdf` tree
(confirmed via `git status`), rebuilt all three suites clean, and ran the
full battery once:

| suite | test cases | failed | assertions | failed assertions |
|---|---|---|---|---|
| semantics | 2767 | 1 | 13343 | 2 |
| backend_ir | 1646 | 46 | 16428 | 137 |
| compile_run | 2679 | 5 | 15294 | 8 |

Identical to every prior session's recorded numbers for this exact
baseline - no drift. `git stash pop` restored the harness, rebuilt clean,
and ran the audited battery once with `PRIMESTRUCT_RECEIVER_TARGET_DIFF_AUDIT=1`
set:

| suite | test cases | failed | [receiver-target-diff-audit] MISMATCH lines | failing-name diff vs baseline |
|---|---|---|---|---|
| semantics | 2767 | 1 | 0 | **empty** |
| backend_ir | 1646 | 46 | 0 | **empty** |
| compile_run | 2679 | 5 | 0 | **empty** |

Then ran two more full 3-suite passes with the env var unset (default
production behavior, which this round does not change at all) - both also
produced sorted failing-test-case-*name* sets byte-identical to the
baseline in all three suites (`diff` empty in every pairwise comparison:
baseline vs. run 1, baseline vs. run 2, run 1 vs. run 2, per suite - 9
comparisons total, all empty). Before trusting each `compile_run` result,
checked for a concurrent instance via `ps aux | grep
'\./PrimeStruct_compile_run_tests' | grep -v grep` rather than plain
`pgrep -f PrimeStruct_compile_run_tests` - the latter produced false
positives this round, matching the literal string inside this session's
own shell command lines (e.g. the `pgrep` invocation itself, and earlier
polling-loop commands), not an actual second instance of the test binary;
`ps aux` filtered to the real executed command line correctly showed
either exactly one or zero real instances at every check. No transient
subprocess-level crash noise was observed this round (unlike the F13
harness round's `bad_alloc`/hang artifacts) - all runs completed cleanly
to their final `[doctest]` summary line on the first attempt.

**Conclusion.** Zero-divergence proven for the F7 File-family slice; no
new quirk surfaced (the OR-vs-leaf-only equivalence held on the full
corpus, and the 11-name method sets matched exactly). Production behavior
is unchanged this round - the harness is purely observational, as
required. This is the fourth diff-audit harness wired in the
monomorphization stage, after F11's, F9's, and F13/F13b/F13c's own (all
three since migrated for real). Per the established
harness-then-migrate discipline, F7 is **not** migrated for real this
round - that stays a separate future Step 2 round. Remaining scope in Row
F after this round: F0-F6/F8 (minus F1/F2/F3/F5/F6/F8's already-documented
non-fits above), F10, F12/F14-F16 (13 of 17 branches once F7 itself is
later migrated), plus all of Row B/C/G and all of `ir_lowerer`.

## Step 2, monomorphization stage: resolveMethodCallTemplateTarget's F7 File-family slice migrated to classifyReceiverElementFamilyJoint, zero-divergence achieved (2026-09-09)

Migrated the Step 1b-harnessed F7 slice for real, the fourth real
migration in the monomorphization stage (after F11, F9, and
F13/F13b/F13c), same harness-first/migrate-once-proven discipline.

**What changed.** The inline `(typeName == "File" ||
normalizedReceiverLeafName == "File") && isFileMethodName(normalizedMethodName)`
gate is replaced by: (1) building the same `ReceiverElementFamilyJointInput`
the Step 1b diff-audit harness was already constructing for observation
(no new computation - the audit-only code is promoted into the primary
path, with `isTemplateShaped=true`/`templateShapedBaseName=
normalizedReceiverLeafName`, feeding the classifier the already-known
leaf name directly, same "hand the classifier the already-known answer"
approach F9/F11/F13 all used), (2) one call to
`classifyReceiverElementFamilyJoint`, (3) dispatching to
`preferredFileMethodTarget(normalizedMethodName)` when the classifier's
family verdict is `File` (the harness's own proven equivalence - the
classifier's File family check already uses `isFileHandleMethodName`, the
identical 11-name method set as this file's own `isFileMethodName`
lambda, verified name-for-name in the Step 1b harness round). Downstream
behavior is byte-identical: `preferredFileMethodTarget` is called with
exactly the same argument (`normalizedMethodName`) as before, so path
construction (the `/file/<method>` vs `/File/<method>` split, the
`self`-receiver special case, the >10-arg fallback) is untouched; only the
*classification* moved. The now-dead inline `isFileMethodName` lambda
(no longer called anywhere in the file once the migration promoted the
classifier's own equivalent check) was removed along with it. The Step 1b
diff-audit-harness scaffolding at this call site (the
`isReceiverTargetDiffAuditEnabled()` check, the comparison, the
`[receiver-target-diff-audit] MISMATCH` stderr line, the `assert`) is
removed entirely; net -39/+0 lines in `TemplateMonomorphMethodTargets.cpp`
(26 insertions, 65 deletions across the whole file diff, mostly this
slice). No other part of Row F's still-unmigrated cascade (F0-F6/F8, F10,
F12, F14-F16) was touched, and F9's/F11's/F13's already-migrated code was
left untouched.

**Verification.** Fresh baseline first, not a trusted prior number:
`git stash`'d this round's own edit back to the clean `281de699f` tree
(confirmed via `git status`), rebuilt all three suites clean, and ran the
full battery once:

| suite | test cases | failed | assertions | failed assertions |
|---|---|---|---|---|
| semantics | 2767 | 1 | 13343 | 1 |
| backend_ir | 1646 | 46 | 16428 | 137 |
| compile_run | 2679 | 5 | 15278 | 8 |

(Semantics' single known-flaky pinned test toggled between 1 and 2 failed
assertions across runs this round, same test name every time - see below;
this does not affect the test-case-name-level comparison this document's
discipline is scoped to.) `git stash pop` restored the migration, rebuilt
clean, and ran the full battery two more times:

| suite | test cases | failed | assertions | failed assertions | failing-name diff vs baseline |
|---|---|---|---|---|---|
| semantics | 2767 | 1 | 13343 | 1-2 (flake) | **empty** (both runs) |
| backend_ir | 1646 | 46 | 16428 | 137 | **empty** (both runs) |
| compile_run | 2679 | 5 | 15278 | 8 | **empty** (both runs) |

Test-case counts identical across all three runs (1 baseline + 2
post-migration) in every suite, and `diff` on the sorted failing-test-
case-*name* list per suite came back empty in every pairwise comparison
checked (baseline vs. run 1, baseline vs. run 2, run 1 vs. run 2 - 9
comparisons total across the three suites, all empty). Semantics'
single already-known-flaky pinned test (`semantic product validates
direct return method-like borrowed helper-return experimental soa reads`)
toggled between 1 and 2 failed assertions within that one test case
across the baseline/run1/run2 runs, exactly matching this document's own
recorded "1 known flake" - the *set* of failing test-case names was
identical in all three runs regardless. Before trusting each
`compile_run`/`backend_ir`/`semantics` result, confirmed via `pgrep -af`
against the anchored full binary path (`'^\./PrimeStruct_<suite>_tests$'`,
not a bare substring match, which this round found produces false
"still running" reports against unrelated leftover polling-loop shell
wrappers from earlier sessions in this same sandbox) that exactly one
real instance of the test binary was running, or none, at each check. No
concurrent-run segfault artifact or mid-run crash noise was observed this
round - all six runs (1 baseline + 2 full post-migration passes across
three suites) completed cleanly to their final `[doctest]` summary line.

**Conclusion.** F7 is now migrated for real, alongside F11, F9, and
F13/F13b/F13c - monomorphization now has four call sites delegating to
`classifyReceiverElementFamilyJoint`. No diff-audit-harness scaffolding
remains anywhere in `TemplateMonomorphMethodTargets.cpp` at this point.
Every other Row A/B/C/D/E/F/G call site this document's Step 0 rule table
catalogs still independently re-derives receiver family membership - in
particular 13 of Row F's 17 branches (F0-F6/F8, F10, F12, F14-F16) and all
of `ir_lowerer` remain completely untouched.

## Step 1b, monomorphization stage: fifth diff-audit harness at F12 generic-Soa slice, zero-divergence verified, migration deferred (2026-09-09)

Picked up an in-progress, uncommitted diff for F12 (generic-SOA-receiver
method-name-paired dispatch - `count`/`count_ref`, `toAos`/`toAosRef`,
`get`/`get_ref`, `push`/`reserve`, `ref`/`ref_ref`, all five gated on the
same `isTemplateMonomorphSoaReceiverType(normalizedTypeName)` family
check) left behind by an earlier round of this same session after a
container restart, sitting unstaged on top of the F7-migration commit
(`a86fba854`). Inspected it against this document's own established
pattern (same shape as F7/F9/F13's harnesses) and against the "Headline
finding: Row F's F12/F14 SOA 'asymmetry' is not an inconsistency, it is
dead code" section above (which independently proves F12's guard has no
method-name gating on family membership itself - method name only
selects which of the five dispatch branches runs once the family already
matched - and that F14's guard is a strict, always-losing superset of
F12's). Both lines of evidence agree: unlike F7/File and the semantics-
stage Buffer/FileError families, Soa family membership in the shared
classifier (`classifyReceiverElementFamilyJoint`) carries no method-name
gate of its own, so one classification call made once before all five
branches (sharing the identical family gate) covers all of F12, rather
than needing a separate audit per method-name pair the way some other
slices did. Confirmed on inspection that the harness correctly hands the
classifier the already-normalized base name directly
(`isTemplateShaped=true`, `templateShapedBaseName=normalizedTypeName`,
same "hand over the pre-parsed base" approach F13/F13b/F13c's slice
used), wires the identical `isInternalSoaCollectionTypeName` predicate
wrapper pattern, is purely observational (never changes control flow,
return value, or side effects), and is zero-cost when
`PRIMESTRUCT_RECEIVER_TARGET_DIFF_AUDIT` is unset. All referenced symbols
(`isReceiverTargetDiffAuditEnabled`, `describeReceiverElementFamily`,
`ReceiverElementFamilyJointInput`, `ReceiverElementFamilyPredicates`,
`classifyReceiverElementFamilyJoint`) and headers (`<cassert>`,
`<iostream>`) already exist and are already included in
`TemplateMonomorphMethodTargets.cpp`. Judged sound as written - no edits
needed.

**Verification.** Fresh baseline first, not a trusted prior number:
`git stash`'d this diff back to the clean `a86fba854` tree (confirmed via
`git status`/`git log`), rebuilt all three suites clean, and ran the full
battery once as plain foreground commands (no backgrounding/polling
across tool calls - a discipline this exact round re-learned the hard
way after briefly backgrounding a run mid-session and then having to
discard a `SIGTERM`-contaminated log and rerun clean):

| suite | test cases | failed | assertions | failed assertions |
|---|---|---|---|---|
| semantics | 2767 | 1 | 13343 | 2 |
| backend_ir | 1646 | 46 | 16428 | 137 |
| compile_run | 2679 | 5 | 15278 | 8 |

`git stash pop`'d the F12 diff back, rebuilt all three suites, then ran
the full battery once with `PRIMESTRUCT_RECEIVER_TARGET_DIFF_AUDIT=1`
set: zero `[receiver-target-diff-audit] MISMATCH` lines in any of the
three suites' output, and test-case/assertion counts identical to
baseline in all three. Then, with the env var unset, ran the full battery
two more times (plain foreground, `pgrep -fc` against the anchored full
binary path confirming no concurrent instance before each run):

| suite | failing-name diff vs baseline (rerun 1) | failing-name diff vs baseline (rerun 2) |
|---|---|---|
| semantics | **empty** | **empty** |
| backend_ir | **empty** | **empty** |
| compile_run | **empty** | **empty** |

Sorted failing-test-case-*name* lists (not just counts) came back
byte-identical (`diff` empty) in every comparison: audit-run vs baseline,
rerun1 vs baseline, and rerun2 vs baseline, across all three suites.
Semantics' pinned known-flaky test
(`semantic product validates direct return method-like borrowed
helper-return experimental soa reads`) is the sole failure there
throughout, matching this document's own recorded flake; backend_ir's 46
and compile_run's 5 failing names were likewise identical across every
run.

**Conclusion.** F12's harness is verified sound and committed as-is,
observation-only, no behavior change - the sixth diff-audit harness
landed at a monomorphization call site (after F11, F9, F13/F13b/F13c,
F7), and the first case in this document where an uncommitted harness
diff survived a container restart across sessions rather than being
authored and verified in one sitting. Real migration (promoting the
audit-only classification into the primary dispatch path, the way F7's
round did) is deliberately deferred to a future Step 2 round, not
attempted here, matching this document's own harness-first/
migrate-once-proven discipline. F14 remains untouched and dead, per the
existing proof above - this round re-confirmed rather than re-derived
that finding. Remaining scope in Row F: F0-F6/F8, F10, F14-F16 (12 of 17
branches) plus F12's still-open real migration, and all of `ir_lowerer`.

## Step 2, monomorphization stage: resolveMethodCallTemplateTarget's F12 generic-Soa slice migrated to classifyReceiverElementFamilyJoint, zero-divergence achieved (2026-09-09)

Migrated the Step 1b-harnessed F12 slice for real, the fifth real
migration in the monomorphization stage (after F11, F9, F13/F13b/F13c,
and F7), same harness-first/migrate-once-proven discipline.

**What changed.** The five method-name-paired branches
(`count`/`count_ref`, `toAos`/`toAosRef`, `get`/`get_ref`,
`push`/`reserve`, `ref`/`ref_ref`) each tested their own inline
`isTemplateMonomorphSoaReceiverType(normalizedTypeName)` call before this
change. That call is now made exactly once, ahead of all five branches,
via `classifyReceiverElementFamilyJoint` (the same
`ReceiverElementFamilyJointInput`/`ReceiverElementFamilyPredicates`
construction the Step 1b harness was already building for observation -
`isTemplateShaped=true`, `templateShapedBaseName=normalizedTypeName`,
the `isInternalSoaCollectionTypeName` predicate wrapper unchanged), with
the resulting `bool isGenericSoaReceiver = ... family ==
ReceiverElementFamily::Soa` bound once and referenced by all five
branches in place of their own repeated inline calls - the harness's own
proven finding (Soa family membership carries no method-name gating of
its own in the classifier, unlike Buffer/File) is exactly what licenses
sharing one classification across all five, rather than needing five
separate ones. Downstream behavior in every branch is byte-identical:
the same `isBorrowedSoaReceiver` ternary, the same
`preferredSamePathSoa*MethodTarget` helper calls, the same
`selectHelperOverloadPath` wrapping, the same return values - only the
family-gate *computation* moved. The Step 1b diff-audit-harness
scaffolding at this call site (the `isReceiverTargetDiffAuditEnabled()`
check, the separate `auditPredicates`/`jointInput` construction, the
`[receiver-target-diff-audit] MISMATCH` stderr line, the `assert`) is
removed entirely, since the classifier's verdict is now load-bearing
rather than observational. Net -21 lines in
`TemplateMonomorphMethodTargets.cpp` (32 insertions, 53 deletions in the
whole-file diff, entirely this slice). No other part of Row F's
still-unmigrated cascade (F0-F6/F8, F10, F14-F16) was touched, F14
(proven dead code) was left alone as separate cleanup, and F9's/F11's/
F13's/F7's already-migrated code was left untouched.

**Verification.** Fresh baseline first, not a trusted prior number:
`git stash`'d this round's own edit back to the clean `7596991cc` tree
(confirmed via `git status`/`git log`), rebuilt all three suites clean,
and ran the full battery once as plain foreground commands (no
backgrounding/polling across tool calls; each run was blocked on
in-turn via a `while kill -0 <pid>` loop inside a single Bash
invocation, never by waiting on a cross-turn notification):

| suite | test cases | failed | assertions | failed assertions |
|---|---|---|---|---|
| semantics | 2767 | 1 | 13343 | 2 |
| backend_ir | 1646 | 46 | 16428 | 137 |
| compile_run | 2679 | 5 | 15278 | 8 |

`git stash pop`'d the F12 migration back, rebuilt all three suites
clean, and ran the full battery two more times (plain foreground,
`pgrep -fc` against the anchored full binary path
`'^\./PrimeStruct_<suite>_tests$'` confirming zero concurrent instances
before each check):

| suite | test cases (run1/run2) | failed (run1/run2) | failing-name diff vs baseline (run1) | failing-name diff vs baseline (run2) | run1 vs run2 |
|---|---|---|---|---|---|
| semantics | 2767/2767 | 1/1 | **empty** | **empty** | **empty** |
| backend_ir | 1646/1646 | 46/46 | **empty** | **empty** | **empty** |
| compile_run | 2679/2679 | 5/5 | **empty** | **empty** | **empty** |

Sorted failing-test-case-*name* lists (not just counts) came back
byte-identical (`cmp` exits 0, no output) in all 9 pairwise comparisons
across the three suites (baseline vs. run1, baseline vs. run2, run1 vs.
run2). Semantics' single already-known-flaky pinned test (`semantic
product validates direct return method-like borrowed helper-return
experimental soa reads`) is the sole failure there in every run,
matching this document's own recorded flake; backend_ir's 46 and
compile_run's 5 failing names were likewise identical across every run.

**Conclusion.** F12 is now migrated for real, alongside F11, F9,
F13/F13b/F13c, and F7 - monomorphization now has five call sites
delegating to `classifyReceiverElementFamilyJoint`. No diff-audit-harness
scaffolding remains anywhere in `TemplateMonomorphMethodTargets.cpp` at
this point. F14 remains untouched, proven dead code, separate cleanup.
Every other Row A/B/C/D/E/F/G call site this document's Step 0 rule
table catalogs still independently re-derives receiver family
membership - in particular F0-F6/F8, F10, and F14-F16 (11 of Row F's 17
branches) plus all of `ir_lowerer` remain completely untouched.

### F14 deleted (2026-09-09): proven-dead code removed outright

This round's task: F14 (the `isConcreteExperimentalSoaReceiver`-gated
dispatch in `TemplateMonomorphMethodTargets.cpp`) had been deliberately
left untouched through every prior F7/F9/F11/F12/F13-family migration
round specifically because its own dead-code proof had been written
against F12's *pre-migration* code (the inline
`isTemplateMonomorphSoaReceiverType(normalizedTypeName)` gate). With F12
now migrated onto `classifyReceiverElementFamilyJoint` (previous round,
commit `fb13216f9`), this round's job was to re-derive the proof against
the CURRENT code before deleting anything, per the area's history of
punishing "trust the old proof" shortcuts.

**Re-derivation, against current code, from direct source reading (not a
repro).** F12's `isGenericSoaReceiver` lambda feeds the classifier
`unwrappedElementType = rawElementBaseType = templateShapedBaseName =
normalizedTypeName`, `isTemplateShaped = true`, and an
`isInternalSoaCollectionTypeName` predicate that is exactly
`isTemplateMonomorphSoaReceiverType`. Tracing
`classifyReceiverElementFamilyJoint` for this input: R1 (string) can only
match if `normalizedTypeName == "string"`; R2 (FileError) only if it
equals `"FileError"`; the template-shape block's `VectorLike` check only
if it is `"vector"`/`"array"`. The internal SOA receiver type name
(`templateMonomorphSoaReceiverTypeName()`, a fixed distinct constant) is
none of those, so whenever `isTemplateMonomorphSoaReceiverType(normalizedTypeName)`
is true, none of R1/R2/VectorLike can have matched first, and the
classifier's `isInternalSoaCollectionTypeName` check is the first (and
only) one left standing - it matches unconditionally, returning `Soa`.
This proves, from the classifier's own current logic (not merely by
inference from the harness's earlier zero-divergence result), that
**`isGenericSoaReceiver` is exactly `isTemplateMonomorphSoaReceiverType(normalizedTypeName)`** -
still true post-migration, with no narrowing or widening introduced by
the classifier hand-off.

F14's guard (`isConcreteExperimentalSoaReceiver`) is
`isTemplateMonomorphSoaReceiverType(normalizedTypeName) &&
isExperimentalSoaVectorSpecializedTypePath(resolvedType)` - its first
conjunct is therefore identical to F12's entire gate. `normalizedTypeName`
is assigned once (near the top of the function) and never reassigned
before either call site; `normalizedMethodName` is `const` and likewise
fixed for the whole function. F14's five branches gate on the same six
method-name pairs as F12's five branches (`count`/`count_ref`,
`toAos`/`toAosRef`-family names, `get`/`get_ref`, `push`/`reserve`,
`ref`/`ref_ref`), and every F12 branch `return true`s unconditionally on
a match - no fallthrough. So for any input where F14's guard could hold
and a shared method name could match, F12's identical gate already held
and had already returned, dozens of lines earlier in the same function,
before F14's guard expression (which additionally depends on
`resolvedType`, computed even later) was even evaluated. F14's second
conjunct (`isExperimentalSoaVectorSpecializedTypePath(resolvedType)`)
cannot rescue any reachability - it only narrows an already-unreachable
branch. The same conclusion the sixth Step 0 round reached against the
pre-migration code holds unchanged against the current, classifier-based
code: F14 is unreachable dead code, not merely dead-in-practice.

**Deletion.** Removed all five `isConcreteExperimentalSoaReceiver`-gated
`if` blocks and the `isConcreteExperimentalSoaReceiver` local, replacing
them with an explanatory comment. `isConcreteExperimentalSoaReceiver`
was a local `const bool`, not a shared helper - nothing else referenced
it. Checked whether any other helper became orphaned as a result:
`isExperimentalSoaVectorSpecializedTypePath` and
`isTemplateMonomorphSoaReceiverType` are both still called from many
other call sites across the codebase (`SemanticsValidator*.cpp`,
`TemplateMonomorph*.cpp` and headers) and were left untouched - nothing
else became unused.

**Zero-divergence proof.** Fresh 3-suite baseline taken first: `git
stash`'d the change back to a clean `fb13216f9` tree (`git status`
confirmed clean before stashing), rebuilt
`PrimeStruct_semantics_tests`/`PrimeStruct_backend_ir_tests`/
`PrimeStruct_compile_run_tests`, and ran all three (foreground only, one
at a time). Then restored the change (`git stash pop`), rebuilt clean,
and ran the full battery twice more:

| suite | baseline | run1 | run2 |
|---|---|---|---|
| semantics | 2767 cases / 1 failed / 13343 assertions / 2 failed | identical | identical |
| backend_ir | 1646 cases / 46 failed / 16428 assertions / 137 failed | identical | identical |
| compile_run | 2679 cases / 5 failed / 15278 assertions / 8 failed | identical | identical |

Sorted failing-test-case-*name* sets (not just counts) came back
byte-identical (`diff` empty) across all pairwise comparisons - baseline
vs. run1, baseline vs. run2, and run1 vs. run2 - for all three suites.
Semantics' single already-known flake (`semantic product validates
direct return method-like borrowed helper-return experimental soa
reads`) is the only failure there in every run, matching this document's
recorded flake; backend_ir's 46 and compile_run's 5 failing names were
identical, name-for-name, in every run.

**Conclusion.** F14 is deleted, not merely dead. Deleting genuinely
unreachable code was, as expected, a pure no-op on behavior - byte-
identical failing-test-name sets prove it. Row F now has 16 branches
(F14 removed); of these, F0-F6/F8, F10, F15-F16 (11 branches) remain
unmigrated/uncharacterized for migration, plus all of Row B/C/G and all
of `ir_lowerer`. F11, F9, F13/F13b/F13c, F7, and F12 remain migrated onto
`classifyReceiverElementFamilyJoint` from prior rounds; nothing else in
this round's scope touched them.

## Step 1b, monomorphization stage: remaining Row F branches assessed, none fit the classifier (2026-09-09)

This round's task was to pick the next well-scoped, not-yet-touched Row F
branch (from the 11 remaining after F14's deletion: F0-F6/F8, F10, F15-F16)
and wire a diff-audit harness. Conclusion: **no remaining branch fits**
`classifyReceiverElementFamilyJoint`'s `(type, methodName, templateShape)
-> family` interface. No harness was wired and no code was touched this
round - this is a scoping finding, not a stalled migration.

**F0-F6/F8 and F10 - already assessed and rejected in the F7 harness
round** (see "Why this slice, not F3 or the rest of F0-F16" above, and the
F7 Step 1b section's per-branch rundown). Re-read that assessment in full
this round rather than trusting it secondhand, and re-confirmed each
line against the current source (`TemplateMonomorphMethodTargets.cpp`,
post-F12-migration/F14-deletion): F0/F5 are trivial cascade guards with no
type-family question to answer; F1 classifies a receiver expression's
*literal spelling*, not a resolved type; F2 and F6 need per-call context
(`locals` lookups, `hasDefinitionFamilyPath`/`hasTemplatedDefinitionFamilyPath`
existence checks) outside the classifier's pure `(type, methodName)` input
shape; F3 answers "what type does this receiver have," the converse
question to what the classifier answers; F8 delegates to a different,
already-purpose-built classifier (`CollectionSpellingClassifier`) for a
different question (compat-spelling rejection); F10 is a narrower,
differently-shaped rule (3 hardcoded method names, unconditional `array`
dispatch) than the classifier's VectorLike family (any method name once
the family matches). None of this changed since the F7 round - re-deriving
it against current code did not surface anything new.

**F15 and F16 - newly assessed this round, both rejected for the same
reason as F6/F8/F15's own neighbor conditions: they are not
type-family-classification decisions at all.** Read
`TemplateMonomorphMethodTargets.cpp:877-890` directly (the only two
branches left in the function after F14's deletion):

```cpp
const std::string samePathMethodTarget = resolvedType + "/" + normalizedMethodName;
const std::string receiverHelperLeaf = receiverHelperFamilyLeaf(resolvedType);
if (!receiverHelperLeaf.empty()) {
  const std::string rootedHelperTarget = "/" + receiverHelperLeaf + "/" + normalizedMethodName;
  if (samePathMethodTarget != rootedHelperTarget &&
      !hasDefinitionFamilyPath(samePathMethodTarget) &&
      hasDefinitionFamilyPath(rootedHelperTarget)) {
    pathOut = selectHelperOverloadPath(expr, rootedHelperTarget, ctx);
    return true;                                            // F15
  }
}
pathOut = preferVectorStdlibHelperPath(resolvedType + "/" + normalizedMethodName, ctx.sourceDefs);
pathOut = selectHelperOverloadPath(expr, pathOut, ctx);
return true;                                                  // F16
```

F15 picks between two already-constructed candidate path strings
(`samePathMethodTarget` vs. `rootedHelperTarget`) based purely on whether
`ctx.sourceDefs` (via `hasDefinitionFamilyPath`) actually has a definition
at each literal path - a source-definition-table lookup, not a
`(type, methodName)` classification. `receiverHelperFamilyLeaf` (the
function-local lambda at line 254) is itself a generic path-leaf-extraction
utility (strip everything up to the last `/`, then truncate at the first
`__t`/`__ov`/`<`) with no receiver-family awareness at all - it would
produce the same leaf text for a struct-family receiver as for a
collection-family one. F16 is the unconditional fallback: it always
`return true`s, running `preferVectorStdlibHelperPath` then
`selectHelperOverloadPath` on whatever `resolvedType` already is - there is
no "is this family X" decision left to make by the time control reaches
F16; every actual family-classification decision in this function already
happened at F7/F9/F11/F12/F13/F13b/F13c above. Force-fitting either F15 or
F16 onto the classifier would mean inventing a new interface for
"does a source definition exist at this literal path" (a
`ctx.sourceDefs`/`hasDefinitionFamilyPath` question), which is a
fundamentally different kind of input than the classifier's pure
type/method-name pair - the same shape mismatch F2/F6/F8 already
established, not a new problem.

**Conclusion: Row F's monomorphization-stage scope for this classifier is
exhausted.** Every one of Row F's 16 remaining branches (after F14's
deletion) has now been individually assessed against
`classifyReceiverElementFamilyJoint`'s interface: F7, F9, F11, F12,
F13/F13b/F13c fit and are migrated (5 of the original 17, F14 deleted as
dead code); F0-F6/F8, F10, F15, F16 (11 branches) do not fit, for reasons
that fall into three buckets - (a) not a classification decision at all
(F0/F5/F15/F16), (b) classifies something other than a resolved
type/method pair - literal spelling (F1) or the inverse question, "what
type is this" (F3), or (c) needs a per-call input dimension the classifier
has no slot for - `locals`, definition-existence lookups, or a
distinct-classifier's own scope (F2/F6/F8/F10). This is not a dead end for
TODO-5294 as a whole - Row B/C/G and all of `ir_lowerer` remain completely
untouched and are the real remaining scope - but it does mean Row F
specifically has no more low-risk "clean fit" migrations left for this
particular classifier; extending the classifier's interface to cover F2/F6/
F10/F15/F16's shapes would be a materially larger, riskier undertaking than
this document's "smallest extension" discipline calls for, and is better
scoped as its own future decision (not started this round) than forced in
under this task's harness-then-migrate cadence.

No source file was changed this round; no harness was wired; the 3-suite
battery was not re-run since there is nothing to verify (production code
is byte-identical to `2877b59d0`).

## Step 1b, ir_lowerer stage: candidate branches assessed, none fit the classifier this round (2026-09-09)

Per this document's own stage order - semantics first (both Row A call
sites migrated), then monomorphization (just closed above as
"exhausted" for this classifier's low-risk scope) - this round starts
the same harness-then-migrate discipline for `ir_lowerer`. Re-read Row
G's full G0-G10 cascade
(`resolveMethodCallDefinitionFromExpr`,
`IrLowererSetupTypeMethodCallResolution.cpp`) and Row category G
continued (I)/(II) (`IrLowererSetupTypeReceiverTargetHelpers.cpp`,
`IrLowererSetupTypeCollectionHelpers.cpp`) in full before picking a
slice, per this round's own task instructions.

**Candidate 1: G6's bare-`Name` static-error-family dispatch - looked
like the F7/F11 shape, is actually the F1 shape.** G6
(`IrLowererSetupTypeMethodCallResolution.cpp:848-893`) is the obvious
first candidate: a receiver `kind == Name` not found in `localsIn`,
literally spelled `FileError`/`ImageError`/`ContainerError`/`GfxError`,
dispatched via `preferred*ErrorHelperTarget` gated on a per-family
method-name set (`FileError`: 5 names `why`/`is_eof`/`eof`/`status`/
`result`; the other three: 3 names `why`/`status`/`result` each, no
`eof`/`is_eof`). This superficially matches the classifier's existing
`FileError` family (R2/R2b) closely enough that the `FileError`
sub-case's known 5-vs-4-name gap (`eof` present here, absent from the
classifier's fixed set) is exactly the same quirk this document's Step
0 table already flagged for Row F's F1 - a reassuring sign it was
recognized correctly, not a new finding.

But on inspection this is **not** an F11-shaped fit - it is the F1
shape, which this document's own Step 1b/monomorphization rounds
already rejected, for the same reason here: `receiver->name` is the
receiver **expression's own literal source spelling**, checked directly
against a hardcoded 4-name list, with no `LocalInfo` lookup or type
resolution involved at all (the `localsIn.find(receiver->name) ==
localsIn.end()` guard immediately above it exists specifically to
select *only* the unbound case). The classifier's `unwrappedElementType`
input is documented (see its header) as "must already be normalized
... via the caller's `normalizeBindingTypeName`" - a resolved-type text,
not a raw identifier spelling. F11's own FileError slice fit cleanly
precisely because it operates on `typeName` *after* receiver-type
inference has already run; G6 runs *before* any such inference for
this exact receiver shape (bare unbound `Name`), by construction. Two
further, independent problems compound this: (a) `ImageError`/
`ContainerError`/`GfxError` have no corresponding family in
`ReceiverElementFamily` at all - modeling them would mean adding three
new enum values plus predicate plumbing, not just observing an
existing branch; (b) even restricting to just the `FileError`
sub-case (the one family the classifier does model), doing so here
would repeat the exact scope decision this document already made for
Row F's F1 - deliberately excluded as "a fundamentally different
question than the classifier answers," not merely deprioritized.
Consistent with that precedent, G6 is left unharnessed this round
rather than force-fit.

**Candidate 2: the Receiver/Collection-helper files' Buffer/File
`LocalInfo`-kind assignments - the F3 shape, already rejected for the
same reason.** Every `typeNameOut = "Buffer"`/`typeNameOut = "File"`
assignment in `IrLowererSetupTypeReceiverTargetHelpers.cpp` (RT2a/RT2i,
RT3b-i's `argsPackElementKind` table, RT3b-ii's dereferenced-receiver
lambda) is an **unconditional** kind-driven assignment - `localInfo.kind
== LocalInfo::Kind::Buffer` sets `typeNameOut = "Buffer"` with no
method-name involved anywhere in the check. This is the same shape as
Row F's F3 (the receiver-type-inference sub-cascade), already
explicitly rejected in the monomorphization round as answering "what
type does this receiver have," the converse of what
`classifyReceiverElementFamilyJoint` answers ("what family does an
already-known type/method pair belong to"). None of these assignments
gate on `normalizedMethodName` at all, so there is no `(type,
methodName)` pair to hand the classifier in the first place - not a
narrower version of the right shape, a different question entirely,
exactly per F3's own precedent.

**Candidate 3: G3c-iii/G3d and `IrLowererSetupTypeCollectionHelpers.cpp`'s
`isExplicit*AliasPath` family - path-shape or method-name-only
classification, not `(type-text, methodName)`.** G3c-iii dispatches on
the call's method leaf alone (7 File-handle names, no type check at
all - a "handled elsewhere" silent no-op). G3d's
`routesExplicitVectorCountMethodThrough*` sub-guards and the
`isExplicit*AliasPath`/`isAllowedResolved*DirectCallPath` family in the
CollectionHelpers file all classify **resolved semantic-product path
strings** (`preferredResolvedPath`, `explicitMethodPath`) via prefix
matching and registry-token resolution, not a normalized receiver
type-text paired with a method name. This is a different input shape
than the classifier's contract (`ReceiverElementFamilyJointInput`
takes type text plus method name plus template-shape flag, not a
resolved path string) - forcing it in would mean building a wholly
separate path-classification interface, not reusing this one.

**The key-value-alias-name stub asymmetry (per this round's task
instructions) did not end up mattering for the decision.** None of
the three candidates above route through
`resolveKeyValueHelperAliasName`/`resolveBorrowedKeyValueHelperAliasName`
(the permanent no-op stubs documented in Row category G continued
(II)) closely enough to need to reason about that known asymmetry -
G6 is FileError/ImageError/ContainerError/GfxError-only (no
key-value path at all), and Candidates 2/3 are rejected on shape
grounds before the key-value/vector asymmetry would even become
relevant. Flagged here only to record that the asymmetry was kept in
mind while scoping, per this round's task instructions, not because it
changed the outcome.

**Conclusion.** No branch examined this round is a clean,
no-extension-needed fit for `classifyReceiverElementFamilyJoint`'s
`(type, methodName, templateShape) -> family` interface - the two most
plausible candidates (G6, and the Buffer/File `LocalInfo`-kind
assignments) turn out to be, respectively, the F1 shape and the F3
shape this document already established as out of scope for this
classifier, and the remaining path/method-only classifiers (Candidate
3) need a differently-shaped interface entirely. This is **not** the
same as monomorphization's "exhausted" finding - only the branches most
analogous to the already-migrated File/Buffer/FileError slices were
checked this round, not every remaining branch in Row G/RT/CH (G1-G5,
G7-G10's remaining sub-branches, and the rest of the two helper files'
predicate family are still open, per the Step 0 table's own UNPINNED
markers). No source file was changed, no harness was wired, and the
3-suite battery was not re-run since production is byte-identical to
`e86cd0221` - matching this document's own precedent for a
scoping-only round (see the Row F "exhausted" section above) rather
than a stalled migration.

## Step 1b, ir_lowerer stage: remaining Row G/RT/CH branches assessed, none fit the classifier (2026-09-09, second round)

Continuing directly from the previous round's conclusion ("only the
branches most analogous to the already-migrated File/Buffer/FileError
slices were checked... G1-G5, G7-G10's remaining sub-branches, and the
rest of the two helper files' predicate family are still open"). This
round goes through that explicit remaining list systematically rather
than picking one more candidate, per this round's own task instructions.
Re-read the full G0-G10 cascade, Row category G continued (I)/(II), and
the previous round's rejection reasoning in full before starting.

**G1 (canonical key-value helper name gate,
`IrLowererSetupTypeMethodCallResolution.cpp:419-492`) - method-name-only
gate, no type-text input at all.** `sourceKeyValueMethodHelperName()`
strips a canonical/rooted key-value path prefix from `explicitMethodPath`
and returns the bare method name only if it is one of the 12 canonical
key-value helper spellings (`count`/`count_ref`/`size`/`contains`/
`contains_ref`/`tryAt`/`tryAt_ref`/`at`/`at_ref`/`at_unsafe`/
`at_unsafe_ref`/`insert`/`insert_ref`) - otherwise returns empty and the
whole branch is skipped. The receiver-is-key-value test that gates the
actual dispatch is `receiverHasKeyValueLocalInfo()` (a `LocalInfo` flag
check via `hasKeyValueKinds`) OR `resolveCollectionPairTypeInfo(...)
.isKeyValueTarget` (a from-scratch semantic-lookup call) - neither is a
normalized type-text string the classifier's `unwrappedElementType`
input could stand in for. On top of that shape mismatch, a match here
does not classify and stop: it directly resolves and dispatches
(`resolveDefinitionFamilyByArity` against `canonicalKeyValueHelperPath`),
fusing classification with resolution the way G8 below also does, and a
non-match does not reject - it silently falls through to G2. Three
independent reasons this is not a `(type, methodName)`-pair fit: no
type-text input, fused classification+resolution, and soft (not
authoritative) fall-through semantics.

**G2 (`isExplicitKeyValueMethodAliasPath(explicitMethodPath)`) -
confirms, does not extend, the previous round's Candidate 3 finding.**
Already covered in substance by the prior round's "path-shape or
method-name-only classification, not `(type-text, methodName)`" finding
for the `isExplicit*AliasPath` family; re-verified this round by reading
the surrounding dispatch (semantic-product method-call-target, then
direct-call-target, then bridge-path-choice, arity-dispatched) - all
resolved-path-string lookups, no type-text classification anywhere in
this branch.

**G3/G3a-G3e - resolved semantic-product path-string comparisons, same
shape as G2/G3d, not re-derived branch by branch.** `resolvedPath`
(from `findSemanticProductMethodCallTarget`) is compared against literal
path strings (`/std/collections/soa/to_aos`), method-leaf name sets
(G3c-iii's 7 File-handle names), and dispatched via
`resolveLoweredDefinitionPath`/`blocksSyntheticCollectionFallbackDirectTarget`
- every sub-branch operates on a resolved path or a bare method-name set,
never a `(type-text, methodName)` pair as the classifier requires. G3a's
guard is purely about missing semantic-node-id bookkeeping (not a
classification question at all). Same shape-mismatch conclusion as the
previous round's Candidate 3, generalized to the rest of G3.

**G4 - error-message-selection flag, not a dispatch decision (already
noted as such in the Step 0 rule table; confirmed, not re-derived).**
`allowBuiltinFallback` here is purely which error text a later failure
reports, computed from five classifier-callback booleans; it never picks
a `ReceiverElementFamily` or a definition.

**G5/RT1a-e (`resolveMethodCallReceiverExpr`) - already read and
enumerated in Row category G continued (I); re-confirmed this round.**
RT1a-b are shape/arity guards unrelated to type. RT1c/RT1d compute a
locally-scoped `allowBuiltinFallback` from the *same* five-classifier
formula as G4 (a documented naming collision, not a new branch) gating
only whether a bare-`Entry` receiver silently defers or hard-errors -
still not a family classification.

**G6 - already rejected last round (the F1 shape); re-confirmed, not
re-derived.**

**G7/RT2/RT3 (`resolveMethodReceiverTarget` and
`resolveMethodReceiverTypeFromLocalInfo`) - the full F3-shaped
receiver-type-inference cascade, of which the previous round only
checked the Buffer/File sub-cases. This round checked the rest
(RT2e-h/j-n's Array/Soa/Vector/KeyValue-family `typeNameOut`
assignments, RT3a's struct-type-name-by-namespace-walk fallback, RT3b's
whole `Call`-kind sub-cascade including its args-pack-kind table and
`dereference`-lambda) and they are all the same shape as the
already-rejected Buffer/File case: unconditional kind/shape-driven
`typeNameOut` assignment with zero method-name gating anywhere. This
confirms (rather than merely extends) the previous round's finding: the
entire `resolveMethodReceiverTarget` function, top to bottom, answers
"what type does this receiver have" - the converse of the classifier's
question - with no exception found in the parts left unchecked last
round.**

**G8 (`resolveMethodDefinitionFromReceiverTarget`,
`IrLowererSetupTypeMethodTargetHelpers.cpp`) - a genuinely new function
this round traced for the first time (cross-referenced but not
branch-enumerated in either prior round); the single most classifier-
shaped candidate found this round, and still not a fit.** This is G8's
own function - the "normal dispatch attempt" both prior rounds left
unopened. It takes `(methodName, typeName, resolvedTypePath, defMap)` -
superficially the closest thing in the whole cascade to the classifier's
`(type, methodName) -> family` contract. Its `shouldPreferCanonicalVectorPath`/
`shouldPreferCanonicalKeyValuePath` lambdas even gate on a type-membership
test (`isVectorReceiverTarget`/`isKeyValueReceiverTarget`, themselves
built from `isBuiltinCollectionTypeName`/`isExperimentalCollectionTypeName` -
the "seventh independent family-membership predicate" this document's
Row G continued (II) section already flagged) AND a method-name set
(vector: `count`/`capacity`/`at`/`at_unsafe`/`push`/`pop`/`reserve`/
`clear`/`remove_at`/`remove_swap`; key-value: `count`/`contains`/`tryAt`/
`at`/`at_unsafe`/`insert`) - the same *shape* of gate the classifier's
R3-R6 use. Four independent reasons it still does not fit:
  1. **Different question.** The classifier's VectorLike/KeyValue
     branches (R3, R5) answer "is this element a vector/key-value family
     member" unconditionally (any method name qualifies once the type
     matches). G8's method-name set instead answers "should path
     construction prefer the canonical stdlib path over the receiver's
     own resolved type path" - a narrower, path-selection question. A
     vector-typed receiver calling a method *not* in that list does not
     get rejected/StructOrUnknown here; it keeps using
     `normalizedResolvedTypePath` and proceeds straight to a definition
     lookup regardless - there is no family-classification outcome to
     hand back at all for that case.
  2. **Wrong input text.** `typeName`/`resolvedTypePath` here are already
     G7-resolved *type paths or names* (e.g. `"vector"`, a specialized
     struct path, an import-aliased struct path) - not the classifier's
     `unwrappedElementType`/`templateShapedBaseName` (a normalized
     *binding* type text such as `"vector<i32>"` that the caller's own
     `splitTemplateTypeName` has already parsed). `isVectorReceiverTarget`
     et al. are resolved-path-string classifiers (per the point above,
     the "seventh predicate" already noted, string-shape not
     `LocalInfo`/`Expr`-based) - the same input-shape mismatch the
     previous round's Candidate 3 already established for G3d/
     `isExplicit*AliasPath`, now confirmed for G8's own predicates too.
  3. **Fused with resolution, not a pure classification.** Every path
     through this function ends in an actual `findMethodDefinitionByPath`/
     `defMap` lookup and returns a `const Definition *` (or a specific
     error string) - there is no point where it merely answers "what
     family" and stops; the classifier's contract is deliberately just
     the family verdict, decoupled from resolution.
  4. **Different method-name sets even where the shapes align.** Even
     restricting to the cases that do line up conceptually (vector
     accessor names), G8's vector set additionally includes the five
     vector *mutator* names (`push`/`pop`/`reserve`/`clear`/`remove_at`/
     `remove_swap`) that the classifier's R3 branch does not gate on at
     all (R3 has no method-name gate whatsoever) - so even a
     narrow "harness just the accessor overlap" attempt would not
     observe the same decision boundary as production here.

**G9/G9a-f (receiver-is-itself-a-call recovery cascade) - the same
F3-shaped type-inference question as G7, confirmed for the sub-branches
not previously read in detail.** G9c (`inferStructReturnPathFromReceiverDef`),
G9d (`inferReceiverTypeFromDeclaredReturn`), and G9e
(`resolveReturnInfoKindForPath`) are three independently-coded "what type
does this definition return" inference mechanisms, each retried through
the same `resolveMethodDefinitionFromTypeNameWithAliasFallback` wrapper -
none take a method name as an input to the type decision itself (the
method name is only used afterward, once a `typeName` has already been
settled, to build the retry path) - so this is F3-shaped, not
`(type, methodName)`-joint, same conclusion as G7.

**G10 (final fallback-error-selection block,
`IrLowererSetupTypeMethodCallResolution.cpp:1224-1245`) - a second,
previously-unexamined error-message-selection gate, not a
classification.** Superficially looks `(type, methodName)`-shaped
(`typeName == "vector"` AND the call is a bare `count`/access/mutator-
shaped call), but the three `blocksBuiltinBareVector*` booleans decide
only whether `errorOut` is set to `priorError` or `lookupError` - there
is no `ReceiverElementFamily`, no definition, no path constructed from
this decision at all. Same shape as G4 (error-text selection), not a
dispatch/classification branch - rejected on that basis, not force-fit
despite superficially resembling a `(type, methodName)` pair.

**Remaining `IrLowererSetupTypeCollectionHelpers.cpp` predicate family,
checked for completeness.** `preferredFileErrorHelperTarget`/
`preferredImageErrorHelperTarget`/`preferredContainerErrorHelperTarget`/
`preferredGfxErrorHelperTarget` (G6's own dispatch targets) take
*methodName only* - no type-text input at all, confirming G6's rejection
shape rather than presenting a new one (the "type" is implicit in which
of the four functions the caller already chose to call, based on the
bare receiver spelling). `isBuiltinCollectionTypeName`/
`isExperimentalCollectionTypeName` (the "seventh predicate" from Row G
continued (II)) are resolved-path-string membership tests with no
method-name involvement whatsoever - building blocks G8 composes into
its method-name-gated lambdas above, not classifiers in their own right.
`normalizeCollectionHelperPath`/`canonicalKeyValueHelperPath` are pure
path builders, not decision points.

**Cross-cutting pattern across both `ir_lowerer` rounds (the "call for
whoever picks this up next" this round's task instructions asked for).**
Every branch examined across both rounds - all ten top-level G-rows,
both helper files' full predicate surface - falls into exactly one of
four shapes, none of which is the classifier's `(type-text, methodName)
-> family` contract:
  1. **Receiver-type inference** (G7/RT2/RT3, G9c-e): "what type does
     this receiver/definition have" - the converse question, already
     named F3-shaped in the monomorphization round and now confirmed to
     cover the *entire* Row G continued (I) file, not just its Buffer/
     File cases.
  2. **Resolved-path-string classification** (G2, G3/G3a-e, G8's
     `isVectorReceiverTarget`/`isKeyValueReceiverTarget`,
     `isBuiltinCollectionTypeName`/`isExperimentalCollectionTypeName`,
     the `isExplicit*AliasPath`/`isAllowedResolved*DirectCallPath`
     family): classifies an already-resolved semantic-product or
     definition path string, not a normalized *binding* type text -
     wrong input shape even where the method-name gating looks similar.
  3. **Method-name-only gates with no type input** (G1's
     `sourceKeyValueMethodHelperName`, G3c-iii's File-handle-name set,
     the `preferred*ErrorHelperTarget` family): the "type" is fixed by
     which code path already ran, never carried as an explicit
     classifier input.
  4. **Fused classification+resolution, or pure error-message
     selection, rather than a standalone family verdict** (G1, G8, G4,
     G10): every candidate that does gate on both a type-shaped
     predicate and a method-name set (G1, G8) immediately proceeds to an
     actual `defMap`/`findMethodDefinitionByPath` lookup in the same
     branch rather than returning a family for the caller to act on
     separately, and the two branches that superficially look
     `(type, methodName)`-shaped without doing a lookup (G4, G10) turn
     out to only be selecting which error string to report.
This is a structural observation, not a scoped classifier-extension
request: unlike a missing family (which would be a small, well-defined
extension), shapes 2-4 above are not gaps in what
`classifyReceiverElementFamilyJoint` currently models - they are answers
to different questions than a family classifier answers at all. Handing
`ir_lowerer` a real reuse win would mean either (a) accepting that this
stage's cascade is fundamentally a fused resolve-and-dispatch machine
that a pure classifier cannot cleanly slot into without duplicating its
resolution logic on the other side of the interface, or (b) a much
larger redesign (a resolved-path-string classifier plus a receiver-type
inferencer, as two new, differently-shaped modules) rather than an
extension of this classifier - a design decision for whoever picks this
up next, not something attempted this round per this round's own task
instructions.

**Conclusion.** No production code changed, no harness wired, 3-suite
battery not rerun (production remains byte-identical to the previous
round's baseline, itself unchanged from `59dfd5332`). Unlike
monomorphization's "exhausted" finding (which closed that stage's
low-risk scope entirely), this round *is* the closing round for
`ir_lowerer`'s Row G/RT/CH scope specifically for this classifier's
existing shape: every top-level G-row and every predicate in both helper
files has now been examined across the two `ir_lowerer` rounds, and none
fit. Whether `ir_lowerer` has further low-risk consolidation potential
under a *different*, purpose-built interface (per the pattern above) is
now a scoping question for a future round, not a branch-hunting one.

## Step 1c Scoping: `CanonicalReceiverType` and `resolveReceiverType` (2026-09-10, characterization only, no code wired)

This section is a new phase, not another migration slice. Both stages'
Step 1b sweeps above (monomorphization's Row F "exhausted" conclusion and
`ir_lowerer`'s two-round Row G/RT/CH sweep) independently landed on the
same structural finding: **every remaining unmigrated branch is
receiver-type *inference* ("what type does this receiver have"), the
converse of what `classifyReceiverElementFamilyJoint` answers** ("given a
known type-text/method-name pair, what family is it"). Enlarging the
classifier itself to also do inference would recreate the exact
entanglement this whole investigation exists to untangle. The direction
agreed instead: split receiver-type inference from receiver-family
classification into two distinct, composable concerns - a new, canonical
output shape (`CanonicalReceiverType`) that every stage's own
`resolveReceiverType(<stage-specific input>, stage-context)` inference
function converges on producing, with the existing classifier consuming
that shape instead of a raw `(type-text, templateShape)` pair. This
round's task is to characterize what the three/four already-documented
inference sites (F3, RT2, RT3/G7) actually need from that shape, ground
enough to let a future round implement it without guessing. **No
production code changes this round.** A header-only design sketch,
`include/primec/support/CanonicalReceiverTypeSketch.h`, accompanies this
section - not wired into any build target, not included by any `.cpp`
file, written only to make the field-by-field discussion below concrete.

### Re-grounding: what F3, RT2, and RT3/G7 already established (re-read in full this round)

- **F3** (`TemplateMonomorphMethodTargets.cpp:402-478`, the Step 0 Rule
  Table's "F3 detail" sub-table, F3-N1/N2, F3-L/B/Fl/S, F3-C1-C3d) infers
  `typeName`/`wrappedReceiverTypeName`/`isBorrowedSoaReceiver` for
  `Name`/`Literal`/`BoolLiteral`/`FloatLiteral`/`StringLiteral`/`Call`-kind
  AST receiver expressions, via monomorphization's own binding-type-text
  machinery (`bindingTypeText`, `qualifyImportedCollectionTypeText`,
  `unwrapImportedCollectionReceiverType`, `inferBindingTypeForMonomorph`,
  `inferExprTypeTextForTemplatedVectorFallback`,
  `inferDefinitionReturnBindingForTemplatedFallback`,
  `getBuiltinCollectionName`). Two already-documented quirks a shared
  shape must be able to represent even where a given stage doesn't fill
  every field: (1) F3-C2's `wrappedReceiverTypeName` field asymmetry - it
  updates `typeName`/`isBorrowedSoaReceiver` but leaves
  `wrappedReceiverTypeName` stale, which matters because F6 (the
  wrapper-method-path branch) reads `wrappedReceiverTypeName`
  specifically; (2) two independently-documented override-priority gaps
  (F3-C3b/F3-C3d) where a later assignment can silently overwrite an
  already-computed `typeName` from F3-C1/C2 with no documented priority
  rule. A third fact, not previously framed as a "gap" but directly
  relevant here: **F3-C3a is not receiver-type inference at all** - when
  the receiver is itself a `Call` that resolves to a struct definition, it
  returns an already-fully-resolved `pathOut = resolved + "/" +
  methodName` immediately, bypassing F3's own remaining cascade, F5, and
  the entire F6-F16 family-dispatch cascade, with **no family
  classification step at all**. See the irreconcilable-case finding
  below.

- **RT2** (`resolveMethodReceiverTypeFromLocalInfo`,
  `IrLowererSetupTypeReceiverTargetHelpers.cpp:202-289`) infers
  `typeNameOut`/`resolvedTypePathOut` from an already-resolved
  `LocalInfo` struct (`isFileHandle`, `structTypeName`,
  `errorHelperNamespacePath`, `errorTypeName`, `kind`
  [`Array`/`Vector`/`Value`/`Buffer`/`Reference`/`Pointer`],
  `isSoaVector`, `hasKeyValueKinds`, `referenceToArray/Vector/Buffer`,
  `pointerToArray/Vector/Buffer`, `valueKind`) - i.e. its raw material is
  *already-classified storage-kind metadata*, not an AST `Expr` or a type
  text string. Its output bifurcates `typeNameOut` (a builtin family name:
  `"array"`/`"soa"`/`"vector"`/`"map"`/`"Buffer"`/`"File"`) from
  `resolvedTypePathOut` (a struct definition path) as two structurally
  separate fields, set by disjoint branches - unlike F3, which keeps
  everything in one `typeName` text and defers the builtin-vs-struct
  distinction to a later resolution step. RT2 has no
  `isBorrowedSoaReceiver`-equivalent field anywhere in its output.

- **RT3/G7** (`resolveMethodReceiverTarget`,
  `IrLowererSetupTypeReceiverTargetHelpers.cpp:527-776`) is the function
  Row G's G7 calls; RT3a delegates `Name`-kind receivers to RT2 (via
  `resolveMethodReceiverTypeFromNameExpr`/`resolveMethodReceiverTypeFromLocalInfo`)
  or a struct-type-name-by-namespace-walk fallback
  (`resolveStructTypePathFromName`); RT3b's `Call`-kind sub-cascade
  independently re-derives family membership from `argsPackElementKind`
  (RT3b-i, a **separate, args-pack-scoped kind enum**, not RT2's plain
  `kind`), a `dereference(...)`-wrapped-receiver lambda (RT3b-ii, keyed on
  either `argsPackElementKind` or plain `kind` depending on shape), a
  bare-key-value-access probe (RT3b-iii), a general `inferExprKind` +
  `resolveMethodReceiverTypeNameFromCallExpr` fallback (RT3b-iv),
  String-means-character-access (RT3b-v), and a final
  struct-type-path-from-call-expr fallback (RT3b-vi); RT3c handles every
  other receiver kind via `typeNameForValueKind(inferExprKind(...))`
  unconditionally. Confirmed by the second `ir_lowerer` Step 1b round: the
  entire function, top to bottom, answers "what type does this receiver
  have" with zero method-name gating anywhere - the same converse-question
  shape as F3, just fed by `Expr`+`LocalInfo` instead of monomorphization's
  own binding-type-text machinery. Like RT2, RT3/G7 has no
  `isBorrowedSoaReceiver`-equivalent output field; unlike F3, it has no
  second "wrapped" type-name text either - its wrapped-vs-unwrapped
  handling (RT2j/RT2l/RT2m's `Reference`/`Pointer` `LocalInfo::Kind`
  branches) works directly off `LocalInfo::Kind` rather than a parallel
  type-name string, because `LocalInfo` already distinguishes
  `Reference`/`Pointer` storage-kind before any type-name text exists.

### First-cut `CanonicalReceiverType` field list

See `include/primec/support/CanonicalReceiverTypeSketch.h` for the same
fields as an (unwired, non-compiling-by-design) C++ struct. Summary:

| field | purpose | filled by |
|---|---|---|
| `family` | the eventual `ReceiverElementFamily` verdict | NOT set directly by `resolveReceiverType` itself - see composition note below |
| `collectionBaseName` | builtin family name text (`"vector"`/`"array"`/`"map"`/`"soa"`/`"Buffer"`/`"File"`/a primitive name) | RT2/RT3 natively (their own `typeNameOut`); monomorphization only after classifying its single `typeName` text |
| `resolvedTypePath` | struct/definition path, when the receiver resolved to a struct type rather than a builtin family | RT2/RT3 natively (their own `resolvedTypePathOut`); monomorphization only after classifying `typeName` |
| `isTemplateShaped` / `templateShapedBaseName` | the caller's own `splitTemplateTypeName`-equivalent parse result, matching `ReceiverElementFamilyJointInput`'s existing fields | both stages, via their own template-shape parse (not re-derived by this struct) |
| `templateArgTexts` | parsed template-argument texts (`vector<T>` → `["T"]`, `map<K,V>` → `["K","V"]`) | **aspirational** - neither F3 nor RT2/RT3 currently retain individual parsed arg texts beyond an arg-count check (see sketch file comment); included because the task background names "template args" as part of the required union, not because current code fills it |
| `isWrapped` / `wrappedBaseTypeName` | `Reference<T>`/`Pointer<T>` wrapping facts | F3 via its (asymmetric) `wrappedReceiverTypeName` text; RT2/RT3 via `LocalInfo::Kind`'s `Reference`/`Pointer` variants directly - two structurally different derivations of the same two output fields, not a shared code path |
| `isBorrowed` | SOA/collection borrowed-vs-owned fact, feeding `_ref` method-name selection | F3 only (its `isBorrowedSoaReceiver`); RT2/RT3 never - `ir_lowerer`'s borrowed/owned renaming happens downstream, inside `IrLowererSetupTypeCollectionHelpers.cpp`'s registry-backed helper-name resolution, not as a receiver-type-inference output at all |
| ~~`isArgsPackElement` / `elemSlotCount`~~ | args-pack storage-layout facts | **removed from the struct** - resolved this round (2026-09-10) as belonging to neither `CanonicalReceiverType`'s output nor a new stage-specific input field; see "Open question, resolved" below |

### Mapping each site onto the struct

- **F3 → `CanonicalReceiverType`.** Fills `collectionBaseName` XOR
  `resolvedTypePath` only after `classifyReceiverElementFamilyJoint` is
  handed the raw `typeName` text it infers (F3 itself doesn't natively
  distinguish "builtin family name" from "struct type name" the way RT2
  does - see the composition note below). Fills `isWrapped`/
  `wrappedBaseTypeName` from `wrappedReceiverTypeName`, faithfully
  reproducing F3-C2's asymmetry (or documenting a deliberate fix, a Step 2
  decision, not a Step 1c one) rather than silently smoothing it over.
  Fills `isBorrowed` from `isBorrowedSoaReceiver` directly. Cannot fill
  `templateArgTexts` beyond what `splitTemplateTypeName` already exposes
  (a base name, not a parsed arg list) without new parsing work outside
  this struct's scope. Does not fill `isArgsPackElement`/`elemSlotCount`
  at all under the struct's current field definition, because F3 itself
  never consults args-pack-ness (that's F2's job, run after and
  independent of F3) - see the open question below. **F3-C3a cannot be
  expressed as a `CanonicalReceiverType` at all** - see the irreconcilable
  case below.

- **RT2 → `CanonicalReceiverType`.** The most direct mapping found: RT2's
  own `typeNameOut`/`resolvedTypePathOut` bifurcation maps onto
  `collectionBaseName`/`resolvedTypePath` natively, with no intermediate
  classification step needed (RT2 is itself already closer to producing a
  family-shaped answer than F3 is - it just doesn't carry a family enum,
  only a name string, because it predates this design). `isWrapped`/
  `wrappedBaseTypeName` fill directly from `LocalInfo::Kind`'s
  `Reference`/`Pointer` branches (RT2j/RT2l/RT2m) - no parallel text
  needed, unlike F3. `isBorrowed` is never filled (see field-list note
  above). `templateArgTexts` is never filled - `LocalInfo` doesn't carry
  parsed template-argument texts at all in the characterized branches.
  `isArgsPackElement`/`elemSlotCount` are never filled by RT2 itself
  (RT2's input, `LocalInfo`, is the *plain*-local shape; the
  args-pack-scoped equivalent is a distinct type consulted only by RT3b-i,
  not by RT2).

- **RT3/G7 → `CanonicalReceiverType`.** RT3a delegates straight to RT2's
  mapping above for bound `Name` receivers. RT3b's `Call`-kind sub-cascade
  is the one site among these three/four that *would* need to fill
  `isArgsPackElement`/`elemSlotCount` if those fields live inside this
  struct, since RT3b-i's entire classification is keyed on
  `argsPackElementKind` rather than plain `LocalInfo::Kind` - this is the
  concrete evidence behind the open question below, not a hypothetical.
  RT3c's `typeNameForValueKind(inferExprKind(...))` fallback maps onto
  `collectionBaseName` alone (a bare value-kind name, e.g. `"i32"`,
  `"string"`), with no template-shape, wrapped, or args-pack facts
  available at that fallback tier at all - all of those fields would stay
  at their sketch-file defaults for that specific fallback path, which is
  fine under the task's own "not every field must be filled" allowance,
  but worth naming explicitly since it's the *narrowest*-filled path
  found across all three/four sites.

### Composition note: where the existing classifier plugs back in

`classifyReceiverElementFamilyJoint` stays exactly as-is per the task's
own instruction - it is not being widened. The clean seam found this
round: monomorphization's `resolveReceiverType` would end its own
cascade (structurally unchanged from F3's existing logic) by handing its
inferred raw `typeName` text, together with the caller's own
`splitTemplateTypeName` result and the method name, to
`classifyReceiverElementFamilyJoint` exactly as the already-migrated Row A
call sites do today - that call's `ReceiverElementFamilyResult` is what
fills `CanonicalReceiverType::family`/`collectionBaseName` (the parts
`classifyReceiverElementFamilyJoint` is already trusted to decide), with
`resolveReceiverType` filling the rest (`resolvedTypePath`, `isWrapped`,
`isBorrowed`, etc.) itself from information the classifier never sees.
`ir_lowerer`'s `resolveReceiverType` would do the same, but starting from
RT2/RT3's already-bifurcated `typeNameOut`/`resolvedTypePathOut` rather
than a single text needing its own builtin-vs-struct split first. Neither
stage's `resolveReceiverType` needs to duplicate
`classifyReceiverElementFamilyJoint`'s own name-set logic; both call into
it once, at the end of their own stage-specific inference.

### Irreconcilable case found: F3-C3a cannot be expressed as `CanonicalReceiverType` without losing information

Per this round's task instructions to document rather than paper over an
irreconcilable case: monomorphization's **F3-C3a** (receiver is a `Call`
that itself resolves to a struct definition) does not answer "what type
does this receiver have" at all - it short-circuits directly to an
already-fully-resolved method-definition *path* (`pathOut = resolved +
"/" + methodName`), skipping the rest of F3, F5, and the entire F6-F16
family-dispatch cascade. There is no `CanonicalReceiverType` this case
could produce that a caller would then still need to run through
`classifyReceiverElementFamilyJoint` and the family-dispatch machinery
for - doing so would be pure wasted work at best, and at worst wrong
(this case's own resolved path is not necessarily what family-based
dispatch would independently reconstruct, since it comes from resolving
the receiver *call itself*, not from classifying an element type text).

Two ways to represent this were considered and both rejected:

1. **A sentinel "already resolved, skip classification" variant added to
   `CanonicalReceiverType`** (e.g. an optional `alreadyResolvedPath`
   field short-circuiting everything else). Rejected: this smuggles a
   *resolution*-shaped field into what the design's own stated goal
   (background section, bullet 3) requires stay a pure inference/
   classification concern - "resolution/dispatch logic... stays OUTSIDE
   both of these," and F3-C3a's payload is resolution, not a type fact.
2. **`resolveReceiverType` returning some kind of tagged union/`variant`
   of `CanonicalReceiverType` or a resolved-path string.** Rejected for
   the same reason at one remove: it would still mean the "pure
   inference" function sometimes returns a resolution result, just wrapped
   - callers would need to branch on which variant they got before ever
   reaching the classifier, reproducing the exact class → resolution
   fusion this whole document's Step 0 sweep flagged as disqualifying for
   G1/G8/G4/G10 in `ir_lowerer`. Doing the same thing on the
   monomorphization side under a different name would not be a genuine
   fix.

**Conclusion: F3-C3a should stay entirely outside `resolveReceiverType`
and `CanonicalReceiverType`**, exactly where it already lives -
monomorphization's own call-site code should keep performing this check
as a pre-step *before* ever calling into the shared inference function
(mirroring how F2's args-pack-map shape already runs as a wholly separate
pre/post step around F3 today, per the Step 0 Rule Table's own note that
F2 "can override an already-successfully-inferred `typeName`'s dispatch
entirely"). This is not a gap in the design; it is evidence that
monomorphization's existing cascade already has (at least) two resolution
short-circuits (F2, F3-C3a) sitting *around* what is otherwise a
classification problem, and a future Step 2 migration for this stage
needs to keep both of them as call-site logic outside the shared
inference/classification pair, not try to fold them in.

### Open question, deliberately left open: do args-pack storage facts belong inside `CanonicalReceiverType` or in `resolveReceiverType`'s stage-specific input?

Not resolved this round; flagged rather than guessed at, per the task's
own instructions. The evidence pulls in different directions per stage:

- **`ir_lowerer`'s RT3b-i** keys its *entire* `Call`-kind family
  classification off a distinct `argsPackElementKind` (not the receiver's
  own plain `LocalInfo::Kind`) - i.e. "is this receiver an args-pack
  element, and what element kind does it have" is something RT3's own
  inference needs to *consult* before it can decide a family at all. This
  argues for `isArgsPackElement`/`elemSlotCount`-shaped facts living on
  the *input* side of `resolveReceiverType`, alongside (or folded into)
  whatever stage-specific context it already takes, not purely as an
  output fact.
- **Monomorphization's F3**, by contrast, never consults args-pack-ness
  during type inference at all - the args-pack-map shape is handled
  entirely by the sibling function F2 (`resolveIndexedArgsPackMapMethodTarget`),
  which runs *after* F3 finishes and unconditionally discards F3's own
  output when it matches. Nothing in F3's own characterized cascade reads
  or produces an args-pack-element-kind-shaped fact at all - which argues
  the opposite way: that this is a call-site-level pre/post step (like
  F3-C3a above), not part of inference's output contract.

A `resolveReceiverType` interface that is genuinely shared across both
stages needs one answer here, and the two stages' own existing structure
disagrees about which shape is natural. Rather than force a decision from
characterization work alone, this is named as the single concrete
open design question a future implementation round must resolve - most
likely by re-examining whether `ir_lowerer`'s RT3b-i's args-pack-kind
consultation is itself best modeled as part of that stage's own
*stage-specific input* to `resolveReceiverType` (an `argsPackElementKind`
the caller supplies alongside the receiver `Expr`, mirroring how
monomorphization's F2/F3 relationship already keeps args-pack-ness
outside inference proper) rather than adding `isArgsPackElement`/
`elemSlotCount` output fields to `CanonicalReceiverType` at all - which
would make the sketch file's current placeholder fields for these two
facts non-final, pending that decision.

### Open question, resolved (2026-09-10): args-pack storage facts belong in NEITHER `CanonicalReceiverType`'s output NOR a new stage-specific input field

Traced concretely rather than guessed at, per this round's task. The
question as originally framed offered two options - shared output field,
or stage-specific input field - and assumed F2 (monomorphization) and
RT3b-i (`ir_lowerer`) were the same underlying question answered two
different ways. Tracing both functions' actual bodies shows that
assumption is wrong in a way that dissolves the question rather than
forcing a pick between the two offered options.

**RT3b-i is not a separate function consulting an external input - it is
inline inference, already fed by inputs `resolveReceiverType` already has.**
`resolveMethodReceiverTarget`'s `Call`-kind sub-cascade
(`IrLowererSetupTypeReceiverTargetHelpers.cpp:556-705`) is one function,
one cascade: RT3a (`Name`-kind, delegating to RT2), RT3b's args-pack
branch (lines 620-705), RT3b's later dereference/bare-key-value/fallback
branches, and RT3c (the `typeNameForValueKind` fallback) all live in the
same `if`/`else if` chain inside the same function, filling the exact
same `typeNameOut`/`resolvedTypePathOut` output parameters. RT3b-i's own
"is this receiver an args-pack access" test
(`localIt->second.isArgsPack`, line 627) reads a field directly off the
`LocalInfo` the function already looked up from the `LocalMap` parameter
it already takes - `argsPackElementKind` (line 18 of
`IrLowererSharedTypes.h`) is likewise already sitting on that same
`LocalInfo`. Nothing new needs to reach the function from outside; the
"input" the open question worried about is already part of the existing
`Expr`+`LocalMap` input shape `resolveReceiverType` would take regardless
of args-pack handling. And RT3b-i's args-pack branches terminate exactly
like every other branch in the cascade - by setting `typeNameOut`/
`resolvedTypePathOut` and returning `true` - so nothing downstream ever
receives a separate "this came from an args-pack" signal either;
confirmed by grepping `ReceiverElementFamilyClassifier.{h,cpp}` for
`isArgsPack`/`ArgsPackElement`: zero matches. The classifier that
`resolveReceiverType`'s output eventually feeds never consults
args-pack-ness at all. The fact is produced, consumed, and fully resolved
into an ordinary `collectionBaseName`/`resolvedTypePath` answer entirely
*inside* inference - it has no reason to be a named field on either side
of the interface.

**Monomorphization's F2 is not RT3b-i's counterpart - it is F3-C3a's.**
`resolveIndexedArgsPackMapMethodTarget`
(`TemplateMonomorphMethodTargets.cpp:322-372`, invoked at line 474, after
F3's entire receiver-type-inference cascade at lines 397-473 has already
run) never reads any of F3's outputs. It independently re-looks-up the
pack receiver (`receiver.args.front()`) in `locals` and re-derives its
own `elemType`/`keyType`/`valueType` via `getArgsPackElementType`/
`extractKeyValueCollectionTypesFromTypeText` (lines 333-345), entirely
disjoint from F3's `typeName`/`wrappedReceiverTypeName`/
`isBorrowedSoaReceiver`. When F2 matches, it computes its own `pathOut`
(line 369) and returns `true` directly - short-circuiting
`classifyReceiverElementFamilyJoint` and the entire F6-F16
family-dispatch cascade exactly the way F3-C3a does, for the same
structural reason: it answers "what is the fully resolved method-target
path for this call", a *resolution* question, not "what type does this
receiver have", an *inference* question. F2 is a second instance of the
F3-C3a shape, not a divergent implementation of RT3b-i's shape - the
design doc's own note under F3-C3a already observed this in passing
("mirroring how F2's args-pack-map shape already runs as a wholly
separate pre/post step around F3 today") without yet drawing the
conclusion that this makes F2 (not RT3b-i) its true structural sibling.

**Conclusion: `isArgsPackElement` and `elemSlotCount` are removed from
`CanonicalReceiverTypeSketch.h` outright, not retained as placeholders on
either side of the interface.** Two independent reasons, one per field:

- `elemSlotCount` is not a receiver-type-inference fact in either stage to
  begin with. In `ir_lowerer` it is computed exclusively inside
  `IrLowererAccessTargetResolution.cpp`, onto `ArrayVectorAccessTargetInfo`
  (`IrLowererCallHelperTypes.h:75-86`) - a distinct struct for a distinct,
  downstream *call/access-target-resolution* concern (codegen slot
  layout, consumed by `IrLowererIndexedAccessEmit.cpp`,
  `IrLowererLowerStatementsCallsStep.cpp`, etc.), never appearing anywhere
  in RT2/RT3's own file. Monomorphization has no elemSlotCount-shaped
  concept anywhere. Nothing in receiver-type inference, in either stage,
  ever needs to produce or consume it.
- `isArgsPackElement` is fully internal to `ir_lowerer`'s inference (see
  above - already derivable from the existing `Expr`+`LocalMap` input,
  never surfaced to any consumer) and, on the monomorphization side, is
  the wrong stage's field entirely - the thing that needs representing
  there is F2's pre-step/short-circuit status, which is exactly the
  F3-C3a shape already ruled out of `CanonicalReceiverType` above, not a
  type-inference fact.

**Practical implication for a future implementation round:** a
monomorphization-side `resolveReceiverType` needs to keep F2 as call-site
logic that runs *before* (or instead of) calling into
`resolveReceiverType` at all for a given call site - same guidance as
F3-C3a, and for the same reason. An `ir_lowerer`-side `resolveReceiverType`
needs no special accommodation for args-pack receivers beyond passing it
the `Expr` and `LocalMap` it needs anyway; RT3b-i's args-pack branches
port into its body unchanged, reading `LocalInfo::isArgsPack`/
`argsPackElementKind` exactly as they do today, with no new parameter and
no new output field. Both stages' `resolveReceiverType` end up with the
identical `CanonicalReceiverType` output shape after this resolution -
the struct did not grow to accommodate either stage's args-pack handling,
which is the strongest evidence available that this was a genuine
non-issue once traced, not two stages disagreeing about a real shared
concern.

### Final sanity pass over the full sketch (2026-09-10, this round)

Re-read `CanonicalReceiverTypeSketch.h` fresh against F3, RT2, RT3/G7, and
F2's characterizations together (not just the args-pack field in
isolation), specifically looking for any other field or asymmetry the
prior round's mapping might have missed:

- `family`/`collectionBaseName`/`resolvedTypePath` - re-verified RT2's
  `typeNameOut`/`resolvedTypePathOut` bifurcation
  (`IrLowererSetupTypeReceiverTargetHelpers.cpp:539-540`) and F3's single
  `typeName` text (`TemplateMonomorphMethodTargets.cpp:397`) against the
  struct; the composition note's account of both still matches the code
  read this round. No change.
- `isTemplateShaped`/`templateShapedBaseName`/`templateArgTexts` - no new
  evidence found this round that either stage retains parsed template-arg
  texts beyond an arg-count probe; the "aspirational" framing already in
  the sketch still holds.
- `isWrapped`/`wrappedBaseTypeName` - re-confirmed F3's asymmetric
  `wrappedReceiverTypeName` (set at lines 401, 416, 450, 460 - the C2
  fallback at line ~422-425 is the one path that leaves it stale) against
  RT2/RT3's direct `LocalInfo::Kind::Reference`/`Pointer` derivation;
  matches the sketch's existing note, no change needed.
- `isBorrowed` - re-confirmed `isBorrowedSoaReceiver` is set at every F3
  assignment site (lines 402, 417, 424, 451, 461) and that RT2/RT3 never
  set an equivalent field anywhere in
  `IrLowererSetupTypeReceiverTargetHelpers.cpp`; matches the sketch's
  existing note.
- Double-checked and confirmed NOT a gap: whether RT3b-i's `FileError`/
  `File` args-pack branches (lines 628-641) need a field beyond
  `collectionBaseName` to represent "resolved via an args-pack access" -
  they don't, since (as established above) nothing downstream of
  `resolveReceiverType`'s output ever needs that fact separately from the
  type name itself.

**No further gaps found this round.** The struct, as amended by this
round's args-pack-field removal, is judged ready for a future
implementation round to build against as-is; the remaining open item is
not a missing field but the F3-C3a/F2 call-site-logic carve-out already
documented above (unchanged from last round), which by design lives
outside this struct rather than inside it.

### Ready to implement: checklist for the next round

This round concludes the design-scoping phase for `CanonicalReceiverType`
- both design questions Step 1c set out to resolve (the F3-C3a
irreconcilable case, and this round's args-pack-fact placement question)
now have grounded answers, and the final sanity pass above found no
further gaps. A future implementation round should, in order:

1. **Move `CanonicalReceiverTypeSketch.h`'s content into a real,
   compiling header** (e.g. `include/primec/support/CanonicalReceiverType.h`),
   replacing the placeholder `int family` with the real
   `primec::ReceiverElementFamily` enum, deleting the sketch-only
   commentary, and wiring it into exactly one build target to start -
   whichever of the two stages goes first (see next item).
2. **Pick one stage to implement first** - `ir_lowerer`'s RT2/RT3/G7 is the
   better first candidate: it already bifurcates `typeNameOut`/
   `resolvedTypePathOut` the way the struct wants, needs no new
   builtin-vs-struct classification step the way F3 does, and per this
   round's finding needs zero new machinery for args-pack handling -
   the smallest first slice. Implement `resolveReceiverType` for that
   stage ONLY, as a new function living alongside the existing
   `resolveMethodReceiverTypeFromLocalInfo`/`resolveMethodReceiverTarget`
   - do not delete or modify those yet.
3. **Harness it exactly the way the classifier migrations were harnessed**
   (see the F7/F9/F11/F13 "Wiring mechanics"/"Verification" write-ups
   above for the pattern): an `isReceiverTargetDiffAuditEnabled()`-gated
   block that runs the new `resolveReceiverType` *alongside* the existing
   ad hoc inference logic it is meant to replace, compares every field of
   its `CanonicalReceiverType` output against what the existing code path
   would have produced (not just the final family verdict - `isWrapped`/
   `isBorrowed`/`resolvedTypePath` too, since those are exactly the fields
   this design added beyond what the classifier alone covers), and
   logs/asserts on any divergence. Purely observational; production's own
   existing code path stays untouched and authoritative during this
   phase.
4. **Prove zero-divergence across the full 3-suite battery** (semantics/
   backend_ir/compile_run) with the audit flag set, using the same
   fresh-baseline-first discipline documented in this file's other
   "Verification" sections (stash to a clean baseline, run once to record
   baseline failing-test-name sets, then run the harnessed build and diff
   the failing-name sets, not raw pass/fail counts, since this project's
   suites carry pre-existing unrelated failures) - only once this is
   clean should the next step touch dispatch.
5. **Only after zero-divergence is proven**, wire the real call site
   (starting with the narrowest one - e.g. RT3a's `Name`-kind delegation
   to RT2, which per this round's mapping is the most direct fit) to
   consume `resolveReceiverType`'s `CanonicalReceiverType` output instead
   of the raw `(type-text, templateShape)` pair it builds today, and
   retire the harness for that call site. Repeat per call site, not all
   at once - this document's own Step 1b migrations (F7 then F9/F11 then
   F13) already established that one-clean-fit-at-a-time is the safe
   cadence here.
6. **Keep F3-C3a and F2 (monomorphization) as call-site pre-steps outside
   `resolveReceiverType`/`CanonicalReceiverType`**, per both this round's
   and last round's findings - do not attempt to fold either into the
   shared struct or function when monomorphization's turn comes.
7. Do not implement both stages' `resolveReceiverType` in the same round;
   this document's own Step 0/Step 1b history (one clean-fit slice per
   round, verified end-to-end before starting the next) is the pattern to
   keep following here too.

## Step 1c, first implementation round: `resolveReceiverType` for `ir_lowerer`'s RT2, harnessed, zero-divergence achieved (2026-09-10)

Following the previous round's "Ready to implement" checklist in order.
This round implements exactly checklist items 1-4: promote the sketch to a
real header, implement RT2's `resolveReceiverType` (the "smallest first
slice" the checklist identified), harness it observationally, and prove
zero-divergence. Items 5-7 (real call-site migration, F3, and doing both
stages in one round) are explicitly **not** attempted this round.

### What got promoted from sketch to real code

`include/primec/support/CanonicalReceiverTypeSketch.h`'s content moved into
a new, real, compiling header,
`include/primec/support/CanonicalReceiverType.h` (`primec::CanonicalReceiverType`),
replacing the placeholder `int family` with the real `ReceiverElementFamily`
enum (via `#include "primec/support/ReceiverElementFamilyClassifier.h"`)
and stripping the sketch-only "not wired into any build target" framing -
the struct is now a genuine, header-only data type included from production
code (`src/ir_lowerer/IrLowererSetupTypeHelpers.h`, and the corresponding
`include/primec/testing/IrLowererHelpers.h`/`ir_lowerer_helpers/IrLowererSetupTypeHelpers.h`
testing mirror). The old sketch file is left in place, untouched, as
historical record of the design-scoping round; nothing references it
anymore.

`ir_lowerer`'s `resolveReceiverType(const LocalInfo &localInfo,
CanonicalReceiverType &out) -> bool` is implemented in
`IrLowererSetupTypeReceiverTargetHelpers.cpp`, immediately after
`resolveMethodReceiverTypeFromLocalInfo` (RT2). It is a deliberately
**independent reimplementation** of RT2's cascade - not a thin wrapper
delegating to RT2 and copying its two out-parameters into the struct - so
that the diff-audit harness (below) is a genuine two-implementation
comparison, not a tautology that could hide a shared bug. It fills
`collectionBaseName`/`resolvedTypePath` exactly as RT2's own
`typeNameOut`/`resolvedTypePathOut` bifurcation does, and additionally
fills `isWrapped`/`wrappedBaseTypeName` (true whenever the successful
resolution came from a `LocalInfo::Kind::Reference`/`Pointer` branch -
RT2 itself tracks no such fact, so this is new information, not something
diffed against). Per `CanonicalReceiverType.h`'s own documented rationale,
`family`, the template-shape fields, and `isBorrowed` are left at their
struct defaults - `family` because RT2 has no method name to hand
`classifyReceiverElementFamilyJoint` (that composition is a future
call-site-level concern, per the prior round's composition note); the
others because RT2's own `LocalInfo`-only input carries neither fact at
all.

One genuine implementation subtlety worth recording (not a divergence, a
faithful-reproduction note): RT2's own cascade checks `!structTypeName.empty()`
unconditionally as its *second* branch, before any `LocalInfo::Kind` check -
so a `Reference`/`Pointer`-kind local with a populated `structTypeName`
resolves there, not in the later `kind == Reference && !structTypeName.empty()`
branch (which is dead code in RT2 as written, always pre-empted by the
earlier check). `resolveReceiverType` reproduces this exact branch order
and does not mark `isWrapped` for that early branch (only the
`Kind`-gated branches below it set `isWrapped`) - a deliberate choice, not
an oversight: RT2 has no field there to disagree with, so there is nothing
for the harness to diff on this point either way.

### What got harnessed and how

`resolveMethodReceiverTypeFromLocalInfo` (RT2) itself is **unmodified** in
every computed value and every `return`'s control flow. The only addition:
one `auditReceiverTypeAgainstLocalInfo(localInfo, <result>, typeNameOut,
resolvedTypePathOut)` call inserted immediately before each of its ~14
`return` statements - the same "one audit call per existing return"
wiring-mechanics pattern the classifier's Step 1b harnesses used at
`resolveArgsPackElementMethodTarget` and the monomorphization call sites.
`auditReceiverTypeAgainstLocalInfo` (a file-local helper in
`IrLowererSetupTypeReceiverTargetHelpers.cpp`) is gated by the existing
`isReceiverTargetDiffAuditEnabled()` (`PRIMESTRUCT_RECEIVER_TARGET_DIFF_AUDIT`
env var) - a no-op, single boolean check, when unset. When set, it
independently calls `resolveReceiverType(localInfo, canonical)` fresh and
compares `canonical.collectionBaseName`/`canonical.resolvedTypePath`/the
boolean result against the values RT2 is about to return, logging a
`[receiver-target-diff-audit] MISMATCH (RT2/resolveReceiverType): ...` line
to stderr (plus a debug-only `assert`, a no-op in this Release build) on
any disagreement. `isWrapped`/`isBorrowed`/`family`/template-shape fields
are **not** part of this comparison - RT2 produces no equivalent value for
any of them (see above), so there is nothing on the legacy side to diff
against; this is the same "some fields aren't filled by a given stage, and
that's fine" allowance the design doc's own field-list table already
documents, not a gap in the harness.

### Zero-divergence proof

Fresh baseline taken via `git stash -u` to a clean tree, rebuilt, and run
(foreground, one suite per call) before any of this round's code existed:

| suite | test cases | failed | assertions | failed |
|---|---|---|---|---|
| semantics | 2767 | 1 | 13343 | 2 |
| backend_ir | 1646 | 46 | 16428 | 137 |
| compile_run | 2679 | 5 | 15278 | 8 |

All three match this document's already-recorded pre-existing baseline
(the same single `soa reads` semantics flake, the same 46 `backend_ir`
names, the same 5 `compile_run` names - `map`-conformance-related, not
receiver-target-related). `git stash pop` restored this round's changes;
rebuilt clean (no warnings/errors); reran the same battery with
`PRIMESTRUCT_RECEIVER_TARGET_DIFF_AUDIT=1`:

| suite | test cases | failed | assertions | failed | `[receiver-target-diff-audit]` MISMATCH lines |
|---|---|---|---|---|---|
| semantics | 2767 | 1 | 13343 | 2 | **0** |
| backend_ir | 1646 | 46 | 16428 | 137 | **0** |
| compile_run | 2679 | 5 | 15278 | 8 | **0** |

Zero mismatch lines in all three suites, identical failure counts to
baseline - `resolveReceiverType` agrees with `resolveMethodReceiverTypeFromLocalInfo`
on every call made across the full battery on the first attempt; no
classifier/implementation iteration was needed.

### Unchanged-default-behavior proof

With the env var unset (default), ran each suite **twice** (foreground,
one call each) and diffed the sorted set of failing test-case *names*
against the freshly-taken baseline above (not just pass/fail counts):

- `PrimeStruct_semantics_tests`: both reruns' failing-name set exactly
  `{"semantic product validates direct return method-like borrowed
  helper-return experimental soa reads"}`, matching baseline - `diff`
  empty both times.
- `PrimeStruct_backend_ir_tests`: both reruns' 46-name failing set
  identical to baseline - `diff` empty both times.
- `PrimeStruct_compile_run_tests`: both reruns' 5-name failing set
  (`C++ emitter runs canonical map reference string access`, `map
  wildcard import rejects stdlib-owned surface in C++ emitter`, `runs
  collection literals with map at in C++ emitter`, `runs vm canonical map
  reference string access with imported canonical helpers`, `runs vm
  shared stdlib map conformance harness`) identical to baseline - `diff`
  empty both times.

Total test-case counts are unchanged in all three suites (no test files
added or modified this round). Production behavior of
`resolveMethodReceiverTypeFromLocalInfo` - the only function any real call
site still calls - is proven unchanged by construction (its own code is
untouched) and confirmed unchanged in practice by the byte-identical
failing-name sets across all 6 post-change runs (2 reruns × 3 suites).

### What this round deliberately did not do

Per the checklist's own explicit ordering and "one stage per round"
discipline:

- No real call site was migrated to consume `resolveReceiverType`'s
  output - `resolveMethodReceiverTypeFromLocalInfo` remains the sole
  production code path everywhere it's called. That is checklist item 5,
  for a future round.
- Monomorphization's F3-side `resolveReceiverType` was not attempted -
  checklist item 7 ("don't implement both stages in one round").
  Monomorphization's F3-C3a and F2 call-site pre-steps remain exactly
  where the prior round's design work placed them (outside
  `resolveReceiverType`/`CanonicalReceiverType` entirely) - unchanged,
  since no monomorphization code was touched this round at all.
- RT3/RT3b/RT3c/G7 (`resolveMethodReceiverTarget`) were not implemented -
  only RT2 (`resolveMethodReceiverTypeFromLocalInfo`), the narrowest slice
  the prior round identified. RT3a's eventual delegation to RT2's mapping
  (the checklist's suggested first real call-site migration target) still
  needs its own round.

### No new quirks or gaps found

Unlike some earlier classifier-migration rounds, this round found no new
production quirk while writing `resolveReceiverType` - RT2's cascade,
traced and re-verified during the prior round's scoping work, ported
directly with zero surprises, and the harness confirmed this empirically
(zero divergence on the first attempt, no iteration needed). The one
implementation subtlety noted above (the dead-code `Reference &&
!structTypeName.empty()` branch, pre-empted by the earlier unconditional
`structTypeName` check) is a faithful reproduction of existing RT2
behavior, not a new finding - the prior round's re-grounding section
already read this cascade in full and did not flag it as a gap, and this
round's independent reimplementation confirms that reading was accurate by
matching it exactly.

### Supersedes/refines, does not contradict, the original Step 1b/Step 2 plan

This section describes a new phase - tentatively **Step 1c** - that sits
alongside, not in place of, the original Step 1b/Step 2 plan. The
`(type, methodName, templateShape) -> family` classifier
(`classifyReceiverElementFamilyJoint`) and its 8 already-landed migrations
across semantics/monomorphization remain exactly as delivered - correct,
narrow, and load-bearing production behavior - and are not touched or
widened by anything in this section. Step 1c is additive scope for the
inference-shaped branches Step 1b's own exhaustive sweeps found the
classifier structurally cannot (and should not) absorb. See the updated
"Plan" section above for where Step 1c sits in the document's own
numbering.

## Step 1c, real call-site migration: RT2's `resolveMethodReceiverTypeFromLocalInfo` deleted, `resolveReceiverType` now sole production path (2026-09-10)

Checklist item 5 from the prior round ("migrate the real call site") is
completed this round. `resolveMethodReceiverTypeFromNameExpr` - the sole
production caller of RT2 - now calls `resolveReceiverType(it->second,
canonical)` directly and copies `canonical.collectionBaseName` /
`canonical.resolvedTypePath` into its own `typeNameOut` /
`resolvedTypePathOut` out-parameters. The old 17-branch
`resolveMethodReceiverTypeFromLocalInfo` function and its observational
`auditReceiverTypeAgainstLocalInfo` diff-audit harness (both described in
the section above) are deleted outright from
`IrLowererSetupTypeReceiverTargetHelpers.cpp`, along with their forward
declarations in both `src/ir_lowerer/IrLowererSetupTypeHelpers.h` and the
testing mirror `include/primec/testing/ir_lowerer_helpers/IrLowererSetupTypeHelpers.h`.
`resolveReceiverType` itself is untouched at the logic level - only its
header comment and the header comment on `CanonicalReceiverType.h` were
updated to describe it as the sole production implementation rather than
an audited sibling. A whole-repo grep for
`resolveMethodReceiverTypeFromLocalInfo` after the deletion turns up only
comments/prose (this doc, `docs/todo.md`, and the
`CanonicalReceiverTypeSketch.h` historical file) - no remaining call
sites or declarations anywhere.

The one direct unit test exercising RT2 by name
(`test_ir_pipeline_validation_ir_lowerer_uninitialized_type_helpers_build_bundled_program_entry_return_runtime_and_setup.cpp`)
was updated with a small file-local adapter,
`resolveReceiverTypeAsLegacyOutParams`, that calls `resolveReceiverType`
and re-exposes its `CanonicalReceiverType` output as the old
`(typeNameOut, resolvedTypePathOut)` two-out-parameter shape the test
cases were written against - preserving the exact same coverage (every
`LocalInfo` shape the old test drove) without rewriting the test bodies
themselves.

The shared `isReceiverTargetDiffAuditEnabled()` /
`PRIMESTRUCT_RECEIVER_TARGET_DIFF_AUDIT` env-gate helper in
`src/support/ReceiverElementFamilyClassifier.cpp`/`.h` was deliberately
left in place (not deleted this round) - it is shared infrastructure used
by the harness pattern across this whole consolidation effort, and RT3/G7
audits in future rounds are expected to reuse it. It has no remaining
caller in production code as of this round (RT2's harness was its last
user), but removing genuinely shared, reusable scaffolding is out of scope
for a single call-site migration and is left for whoever lands the next
harness or for a dedicated cleanup pass if it turns out nothing ever reuses
it.

### Verification

Fresh baseline via `git stash -u` back to clean `b617485fd` (the last
commit before this migration), rebuilt, all three suites run foreground
(one call each; `PrimeStruct_compile_run_tests` run detached-and-`wait`ed
on its own PID within a single foreground call, since its runtime exceeds
the harness's single-call cap):

| suite | test cases | failed | assertions | failed |
|---|---|---|---|---|
| semantics | 2767 | 1 | 13343 | 2 |
| backend_ir | 1646 | 46 | 16428 | 137 |
| compile_run | 2679 | 5 | 15278 | 8 |

Identical counts and names to every prior round's recorded baseline.
`git stash pop` restored the migration; rebuilt clean (no warnings or
errors); ran the same battery **twice more** (foreground only, same
per-suite methodology):

| run | semantics failed | backend_ir failed | compile_run failed |
|---|---|---|---|
| baseline | 1 | 46 | 5 |
| run 1 (migration applied) | 1 | 46 | 5 |
| run 2 (migration applied) | 1 | 46 | 5 |

For all three suites, the sorted set of failing test-case *names* (not
just counts) was diffed pairwise - baseline vs. run 1, baseline vs. run
2 - and came back byte-identical (`diff` empty) in all six comparisons.
The failing names themselves are the same pre-existing, receiver-target-unrelated
set recorded in every earlier round of this effort (the one `soa reads`
semantics flake, the same 46 `backend_ir` names, the same 5 `map`-conformance
`compile_run` names). No new failures, no fixed failures, no flakes
introduced.

### Net code removed

```
 include/primec/support/CanonicalReceiverType.h                          |  13 +- (comment only)
 include/primec/testing/ir_lowerer_helpers/IrLowererSetupTypeHelpers.h   |   3 - (declaration only)
 src/ir_lowerer/IrLowererSetupTypeHelpers.h                              |  13 -  (declaration + comment)
 src/ir_lowerer/IrLowererSetupTypeReceiverTargetHelpers.cpp              | 159 -  (old function + harness deleted, net)
 tests/.../test_..._build_bundled_program_entry_return_runtime_and_setup.cpp | +18  (legacy-shape adapter added)
```

Net across all five files: **-150 lines** (69 insertions, 219 deletions).
The production `.cpp` file alone nets **-159 lines** (17 insertions, 176
deletions) - the 17-branch duplicate cascade and its diff-audit harness
are gone; `resolveReceiverType` is now the only implementation of RT2's
logic in the codebase.

### What remains unmigrated

Per the design doc's own scope (see "Step 1c Scoping" above): RT3/RT3b/RT3c
and G7 (`resolveMethodReceiverTarget`'s `Call`-kind sub-cascade) are not
touched by this round, and monomorphization's F3 producer for
`CanonicalReceiverType` has not been implemented at all. TODO-5294 remains
open; see `docs/todo.md` for the current per-item status.

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

## Step 1c, RT3b (Call-kind receiver) harness round: `resolveReceiverTypeFromCallExpr`, zero-divergence achieved, NOT migrated (2026-09-10)

This round gives RT3b - the `Call`-kind receiver sub-cascade of
`resolveMethodReceiverTarget` in `IrLowererSetupTypeReceiverTargetHelpers.cpp`
(the one shape "What remains unmigrated" above flagged as untouched) - the
same harness-then-migrate treatment RT2 went through across its own two
rounds. This round is the harness round only, matching RT2's own first
round: a new, independently-written `resolveReceiverTypeFromCallExpr`
function is added alongside the legacy cascade and proven to agree with it
observationally. **No production call site is migrated this round** -
`resolveMethodReceiverTarget`'s own `Call`-kind branch is byte-for-byte
unchanged in its logic; the only addition to it is a scope-exit audit
guard (below) that has zero effect unless
`PRIMESTRUCT_RECEIVER_TARGET_DIFF_AUDIT` is set.

### `resolveReceiverTypeFromCallExpr`

Declared in both `src/ir_lowerer/IrLowererSetupTypeHelpers.h` and the
testing mirror `include/primec/testing/ir_lowerer_helpers/IrLowererSetupTypeHelpers.h`,
defined in `IrLowererSetupTypeReceiverTargetHelpers.cpp`. It takes the
same inputs `resolveMethodReceiverTarget`'s `Call`-kind branch closes over
(`receiverExpr`, `localsIn`, `inferExprKind`, `resolveExprPath`,
`importAliases`, `structNames`, `semanticProgram`, `semanticIndex`) and
writes a `CanonicalReceiverType` instead of the legacy
`(typeNameOut, resolvedTypePathOut)` out-parameter pair - mirroring RT3a's
existing `resolveMethodReceiverTypeFromNameExpr` → `resolveReceiverType`
relationship, but for the `Call`-kind shape.

It is a deliberate, independent reimplementation of the `Call`-kind
branch's control flow, including the args-pack-kind classification for
receivers that are an `isArgsPack` local accessed via a builtin-access/
`at`-shaped call (RT3b-i in the Step 1c Scoping round's terms) - per that
round's "Open question, resolved" finding, this needs no new
`CanonicalReceiverType` field and no new input beyond what `localsIn`
already exposes via `LocalInfo::isArgsPack`/`argsPackElementKind`, so it
is a genuine `resolveReceiverType`-shaped question rather than call-site
logic that has to stay outside the shared function. Like the legacy
branch it mirrors, it always returns `true` - the `Call`-kind branch has
no failure exit anywhere in `resolveMethodReceiverTarget`.

### Wiring: a scope-exit audit guard, not an inline call

The orphaned diff this round started from wrote the function and its
`auditReceiverTypeAgainstCallExpr` comparison helper but had not yet
wired the helper into `resolveMethodReceiverTarget` itself. Both were
otherwise complete and correct on inspection (control flow, `LocalInfo`
field usage, and out-parameter handling all matched the legacy branch
they mirror) - no bugs were found or fixed in the orphaned code this
round, only the wiring gap was closed.

The `Call`-kind branch has many exit points - every one of them is
`return true;`, at various nesting depths, with no shared tail. Rather
than touch each return site (risking an actual behavior change) or
restructure the branch's control flow, the wiring adds a single
scope-exit guard object, declared at the top of the branch:

```cpp
if (receiverExpr.kind == Expr::Kind::Call) {
  struct ReceiverTargetDiffAuditGuard {
    // ...captured-by-reference inputs, plus typeNameOut/resolvedTypePathOut...
    ~ReceiverTargetDiffAuditGuard() {
      auditReceiverTypeAgainstCallExpr(receiverExpr, localsIn, inferExprKind, resolveExprPath,
                                        importAliases, structNames, semanticProgram, semanticIndex,
                                        typeNameOut, resolvedTypePathOut);
    }
  } receiverTargetDiffAuditGuard{ /* ... */ };
  // ...branch body unchanged, every `return true;` left exactly as it was...
}
```

C++ guarantees the guard's destructor runs on every path out of its
enclosing scope, including through nested `if`s, on the way out via any
of the branch's `return true;` statements - so the audit call fires
exactly once per `Call`-kind receiver resolution, with whatever
`typeNameOut`/`resolvedTypePathOut` the legacy logic ended up settling
on, without any of the branch's existing `return` statements being
touched. `auditReceiverTypeAgainstCallExpr` itself only reads
`typeNameOut`/`resolvedTypePathOut` (both taken by `const` reference) and
no-ops immediately when the env var is unset, so neither the guard nor
the audit it runs can change `resolveMethodReceiverTarget`'s return
value, out-parameters, or control flow.

### Verification

Build: clean release rebuild (`-Wall -Wextra -Wpedantic -Werror`) of
`primec_ir_lib` and all three suites below with the harness applied -
no warnings (in particular, no unused-function warning for
`resolveReceiverTypeFromCallExpr`/`auditReceiverTypeAgainstCallExpr`,
confirming the wiring is real, not dead code).

Fresh baseline (`git stash -u` back to `0646d0444`, clean release
rebuild, all three suites run foreground): `PrimeStruct_semantics_tests`
1 failed (`semantic product validates direct return method-like borrowed
helper-return experimental soa reads`), `PrimeStruct_backend_ir_tests` 46
failed, `PrimeStruct_compile_run_tests` 5 failed (the `map`-conformance/
`canonical map reference` cluster) - the same pre-existing,
receiver-target-unrelated names as every earlier round of this effort.

With the harness applied and `PRIMESTRUCT_RECEIVER_TARGET_DIFF_AUDIT=1`
set, all three suites run foreground: **zero `[receiver-target-diff-audit]
MISMATCH` lines** across all of them, and the same failed-test-case
counts/names as the fresh baseline (1 / 46 / 5, identical names) - the
pre-existing failures are unrelated to receiver-target resolution and the
audit assertion did not fire on any of them, so `resolveReceiverTypeFromCallExpr`
agrees with the legacy `Call`-kind cascade on every receiver shape these
three suites exercise.

With the env var unset again, the full three-suite battery was run twice
more (foreground); the failing-test-case **names** were extracted and
diffed byte-for-byte against the fresh baseline and against each other -
identical in all three suites, both reruns. This confirms the scope-exit
guard's presence has no observable effect on `resolveMethodReceiverTarget`
in its default (un-audited) configuration.

### What remains unmigrated (as of the harness round)

`resolveMethodReceiverTarget`'s `Call`-kind branch is still the sole
production implementation of RT3b - `resolveReceiverTypeFromCallExpr` is
proven-equivalent but unused in production. Migrating the call site
(replacing the branch's body with a call to `resolveReceiverTypeFromCallExpr`
plus a copy into the legacy out-parameters, the same shape RT2's own
migration round took) is left for a future round, along with RT3c and G7
generally, and monomorphization's F3 producer. TODO-5294 remains open;
see `docs/todo.md` for current per-item status. **Superseded by the real
migration below - this call site is no longer unmigrated.**

## Step 1c, RT3b real call-site migration: old inline `Call`-kind cascade deleted, `resolveReceiverTypeFromCallExpr` now sole production path (2026-09-10)

This round completes RT3b's harness-then-migrate arc (the harness round
above proved zero-divergence; this round lands the migration), mirroring
RT2's own two-round precedent exactly. `resolveMethodReceiverTarget`'s
`Call`-kind branch is now:

```cpp
if (receiverExpr.kind == Expr::Kind::Call) {
  CanonicalReceiverType canonical;
  resolveReceiverTypeFromCallExpr(receiverExpr, localsIn, inferExprKind, resolveExprPath, importAliases,
                                  structNames, semanticProgram, semanticIndex, canonical);
  typeNameOut = canonical.collectionBaseName;
  resolvedTypePathOut = canonical.resolvedTypePath;
  return true;
}
```

replacing the ~250-line old inline cascade (the args-pack-kind
classification block, the `dereference(...)`-wrapped-receiver lambda, the
bare-key-value-access/`tryAt` probes gating the `inferExprKind` fallback,
and the struct-type-path fallback - RT3b-i through RT3b-vi in the Step 1c
Scoping round's terms). The output translation is the same shape RT2's
migration used for `resolveMethodReceiverTypeFromNameExpr`: copy
`canonical.collectionBaseName` into `typeNameOut` and
`canonical.resolvedTypePath` into `resolvedTypePathOut`, since
`resolveReceiverTypeFromCallExpr` already writes exactly those two
`CanonicalReceiverType` fields (see its own definition above) and nothing
else RT3b's legacy shape needs. `resolveReceiverTypeFromCallExpr` itself
is untouched at the logic level - only its header comments (here and in
`IrLowererSetupTypeHelpers.h`) were updated to describe it as the sole
production implementation rather than an audited sibling.

Also deleted: the `auditReceiverTypeAgainstCallExpr` diff-audit function
and the `ReceiverTargetDiffAuditGuard` scope-exit RAII object that wired
it into the branch (both described in the harness round above) - diffing
the migrated function against itself post-migration is meaningless, the
same pattern as RT2's own harness retirement. This file's now-unused
`#include "primec/support/ReceiverElementFamilyClassifier.h"` (the header
declaring `isReceiverTargetDiffAuditEnabled`, this file's only use of that
header) and its now-unused `<cassert>`/`<iostream>` includes were dropped
along with the harness code that used them. The shared
`isReceiverTargetDiffAuditEnabled()` /
`PRIMESTRUCT_RECEIVER_TARGET_DIFF_AUDIT` env-gate helper itself in
`src/support/ReceiverElementFamilyClassifier.cpp`/`.h` was deliberately
left in place, same rationale as RT2's migration: reusable infrastructure
for a future RT3c/G7 harness, out of scope for a single call-site
migration, even though it currently has no remaining caller anywhere in
the tree.

A whole-repo grep for `auditReceiverTypeAgainstCallExpr` and
`ReceiverTargetDiffAuditGuard` after the deletion turns up no references
outside this doc's own and `docs/todo.md`'s own prose describing the
now-completed migration - no stale call sites, declarations, or dangling
comments anywhere else in the tree. The old inline cascade was never a
separately-named function (it lived directly inside
`resolveMethodReceiverTarget`'s `Call`-kind branch), so there is no
separate symbol to grep for beyond the branch itself, which is now the
migrated code.

### Verification

Fresh baseline via `git stash -u` back to the clean `b422c5378` tree (the
harness round's own commit), rebuilt, all three suites run foreground (one
call each; `PrimeStruct_compile_run_tests` run via `nohup` and `wait`-ed
on by its own PID across as many foreground calls as needed until it
actually exited, since its runtime exceeds the harness's single-call
cap):

| suite | test cases | failed | assertions | failed |
|---|---|---|---|---|
| semantics | 2767 | 1 | 13343 | 2 |
| backend_ir | 1646 | 46 | 16428 | 137 |
| compile_run | 2679 | 5 | 15278 | 8 |

Identical counts to every prior round's recorded baseline (the one `soa
reads` semantics flake, the same 46 `backend_ir` names, the same 5
`map`-conformance `compile_run` names). `git stash pop` restored the
migration; rebuilt clean (no warnings or errors, confirming no leftover
dead code or unused-include diagnostics from the deleted harness).

Ran the same battery **twice more** (foreground only, same per-suite
methodology):

| run | semantics failed | backend_ir failed | compile_run failed |
|---|---|---|---|
| baseline | 1 | 46 | 5 |
| run 1 (migration applied) | 1 | 46 | 5 |
| run 2 (migration applied) | 1 | 46 | 5 |

For all three suites, the sorted set of failing test-case *names* (not
just counts) was diffed pairwise - baseline vs. run 1, baseline vs. run
2, and run 1 vs. run 2 - and came back byte-identical (`diff` empty) in
all nine comparisons. No new failures, no fixed failures, no flakes
introduced.

### Net code removed

```
 src/ir_lowerer/IrLowererSetupTypeHelpers.h                 |   8 +-   9 - (comments only, net -1)
 src/ir_lowerer/IrLowererSetupTypeReceiverTargetHelpers.cpp |  22 +- 308 - (old cascade + harness deleted, net -286)
```

Net across both files: **-287 lines** (30 insertions, 317 deletions). The
production `.cpp` file alone nets **-286 lines** - the ~250-line inline
`Call`-kind cascade and its diff-audit harness are gone;
`resolveReceiverTypeFromCallExpr` is now the only implementation of RT3b's
logic in the codebase, and `resolveMethodReceiverTarget`'s `Call`-kind
branch is 7 lines.

### What remains unmigrated

RT2, RT3/RT3a (delegates to RT2), and RT3b are now all fully migrated -
each has exactly one production implementation, no duplicate cascades, no
harness scaffolding left at any of their call sites. Per the design doc's
own scope: RT3c and G7's `Call`-kind sub-cascade in
`IrLowererSetupTypeMethodCallResolution.cpp` remain untouched, and
monomorphization's F3 producer for `CanonicalReceiverType` has not been
implemented at all. TODO-5294 remains open; see `docs/todo.md` for
current per-item status.

## Step 1c, RT3c harness round: `resolveReceiverTypeFromFallbackExpr`, zero-divergence achieved, NOT migrated (2026-09-10)

This round re-confirmed `resolveMethodReceiverTarget`'s structure post-RT3b
migration: the function is now exactly three branches -
`receiverExpr.kind == Name` (RT3a, delegating to RT2 via
`resolveMethodReceiverTypeFromNameExpr`/`resolveReceiverType`),
`receiverExpr.kind == Call` (RT3b, now `resolveReceiverTypeFromCallExpr`
directly), and a final unconditional fallback for every other `Expr::Kind`
(RT3c) - confirming nothing besides RT3c remains unmigrated in this
function. RT3c's own body is unchanged from the Step 1c Scoping round's
characterization: a single expression,
`typeNameOut = inferExprKind ? typeNameForValueKind(inferExprKind(receiverExpr, localsIn)) : ""`,
followed by an unconditional `return true` (the branch has no failure
exit, matching every other branch in this function except RT3a's).

Rather than moving to G7 this round (deferred - see below), RT3c got the
same harness-then-migrate treatment RT2 and RT3b already went through,
since it is the one remaining un-migrated piece squarely inside this
module's already-agreed ir_lowerer scope (`resolveMethodReceiverTarget`
itself) and finishing it keeps that function fully consolidated before
opening a new file (G7's `IrLowererSetupTypeMethodCallResolution.cpp`) or
a new stage (monomorphization's F3). This round is the harness round
only - the production fallback branch's logic is byte-for-byte unchanged;
the only addition is an audit call that no-ops unless
`PRIMESTRUCT_RECEIVER_TARGET_DIFF_AUDIT` is set.

### `resolveReceiverTypeFromFallbackExpr`

Declared in both `src/ir_lowerer/IrLowererSetupTypeHelpers.h` and the
testing mirror `include/primec/testing/ir_lowerer_helpers/IrLowererSetupTypeHelpers.h`,
defined in `IrLowererSetupTypeReceiverTargetHelpers.cpp` immediately before
`resolveMethodReceiverTarget`. It takes `receiverExpr`, `localsIn`, and
`inferExprKind` (the only three inputs RT3c's own legacy body reads) and
writes only `CanonicalReceiverType::collectionBaseName` - every other
field stays at its struct default, exactly as the Step 1c Scoping round's
RT3/G7 note predicted this would be "the *narrowest*-filled path found
across all three/four sites" (no template-shape, wrapped, resolved-path,
or family facts are available at this fallback tier at all). Always
returns `true`, matching RT3c's "never fails" behavior.

### Wiring: a direct audit call, not a scope-exit guard

Unlike RT3b's `Call`-kind branch (many exit points scattered through a
~250-line cascade, which needed a `ReceiverTargetDiffAuditGuard` RAII
object to fire exactly once regardless of which `return` fired), RT3c's
legacy body has exactly one statement and one `return true;` at the very
end of `resolveMethodReceiverTarget` itself - the simpler direct-call
pattern RT2's own harness round used fits cleanly. A new
`auditReceiverTypeAgainstFallbackExpr` helper (anonymous-namespace,
audit-only) is called with `receiverExpr`, `localsIn`, `inferExprKind`,
and the legacy branch's own `typeNameOut` immediately after that line
computes it, before the shared `return true;`. It no-ops immediately
unless `isReceiverTargetDiffAuditEnabled()` (the same
`PRIMESTRUCT_RECEIVER_TARGET_DIFF_AUDIT`-gated helper in
`src/support/ReceiverElementFamilyClassifier.{h,cpp}` RT3b's harness round
already used and deliberately left in place for a future round); when
enabled it calls `resolveReceiverTypeFromFallbackExpr` independently and
compares its `collectionBaseName` against the legacy `typeNameOut`,
`assert`-ing on any mismatch. It only reads `typeNameOut` (by const
reference) and never writes any out-parameter, so it cannot change
`resolveMethodReceiverTarget`'s return value, out-parameters, or control
flow in either configuration.

### Verification

Build: clean release rebuild (`-Wall -Wextra -Wpedantic -Werror`) of
`primec_ir_lib` and all three suites below with the harness applied - no
warnings (confirming `resolveReceiverTypeFromFallbackExpr`/
`auditReceiverTypeAgainstFallbackExpr` are both real, wired code, not
dead functions).

Fresh baseline (`git stash -u` back to the clean `9d9c10ddc` tree, clean
release rebuild, all three suites run foreground):

| suite | test cases | failed | assertions | failed |
|---|---|---|---|---|
| semantics | 2767 | 1 | 13343 | 2 |
| backend_ir | 1646 | 46 | 16428 | 137 |
| compile_run | 2679 | 5 | 15294 | 8 |

Identical failed-test-case counts/names to every prior round of this
effort (the one `soa reads` semantics flake, the same 46 `backend_ir`
names, the same 5 `map`-conformance `compile_run` names; `compile_run`'s
own assertion *count* varies slightly run-to-run independent of any code
change here - see below, this is pre-existing and unrelated).

`git stash pop` restored the harness; rebuilt clean (no warnings/errors).
With the harness applied and `PRIMESTRUCT_RECEIVER_TARGET_DIFF_AUDIT=1`
set, all three suites run foreground: **zero
`[receiver-target-diff-audit] MISMATCH` lines** across all of them, and
the same failed-test-case counts as the fresh baseline (1 / 46 / 5) -
`resolveReceiverTypeFromFallbackExpr` agrees with the legacy RT3c fallback
on every receiver shape these three suites exercise.

With the env var unset again, the full three-suite battery was run twice
more (foreground). The failing-test-case **names** (not just counts) were
diffed pairwise - baseline vs. run 1, baseline vs. run 2, run 1 vs. run
2 - across all three suites: all nine comparisons came back byte-identical
(`diff` empty). Note: `compile_run`'s own total assertion count was 15294
in the very first (stash-baseline) run and a stable 15278 in both
un-audited reruns after the harness was restored - the same small
run-to-run assertion-count wobble this doc's RT3b round also recorded
(15278 there too), pre-existing and confined to `compile_run`'s own
timing/ordering-sensitive assertions, not to failing-test-case identity
(which was checked and is identical in all nine pairwise diffs) and not
caused by this round's harness (the wobble appears both with and without
the harness present). This confirms the audit call's presence has no
observable effect on `resolveMethodReceiverTarget` in its default
(un-audited) configuration.

### What remains unmigrated (as of the harness round)

`resolveMethodReceiverTarget`'s fallback branch is still the sole
production implementation of RT3c - `resolveReceiverTypeFromFallbackExpr`
is proven-equivalent but unused in production. Migrating the call site
(replacing the branch's body with a call to
`resolveReceiverTypeFromFallbackExpr` plus a copy into the legacy
`typeNameOut` out-parameter, the same shape RT2's and RT3b's own
migration rounds took) is left for a future round. After that migration,
`resolveMethodReceiverTarget` will have exactly zero un-migrated logic
left - RT2, RT3a, RT3b, and RT3c will all be single-production-path. G7's
own `Call`-kind sub-cascade in `IrLowererSetupTypeMethodCallResolution.cpp`
and monomorphization's F3 producer remain untouched. TODO-5294 remains
open; see `docs/todo.md` for current per-item status.

### Why G7 was not started this round

The task offered G7 as this round's candidate if RT3c turned out to be
either already-covered or wrong-shaped for this module. Neither was true:
RT3c was confirmed still un-migrated (see above), and it is squarely
inside this module's already-scoped `resolveMethodReceiverTarget` surface
- finishing the function already in flight, with its harness
infrastructure already in place and proven for two of its three branches,
was judged the lower-risk, higher-continuity choice over opening a new
file this round. G7 itself was re-confirmed as a genuine
`resolveReceiverType`-shaped candidate (its own row in the Step 0/Step 1b
sweep: `resolveMethodReceiverTarget(*receiver, ...)` setting
`typeName`/`resolvedTypePath`, an inference question, not a resolution
one - the same shape RT2/RT3 already are, and the reason it was rejected
as a `classifyReceiverElementFamilyJoint` candidate earlier is exactly
why it fits *this* module) and remains next in line once RT3c's migration
lands.

## Step 1c, RT3c real migration: `resolveMethodReceiverTarget` fully migrated end-to-end (2026-09-10)

Migrates `resolveMethodReceiverTarget`'s final-fallback branch (reached
when `receiverExpr` is neither `Name`- nor `Call`-kind) to call
`resolveReceiverTypeFromFallbackExpr` directly and copy its
`CanonicalReceiverType::collectionBaseName` output into the legacy
`typeNameOut` out-parameter - the same shape RT2's (`0646d0444`) and
RT3b's (`9d9c10ddc`) own migration rounds took. The old one-line inline
body (`typeNameOut = inferExprKind ? typeNameForValueKind(inferExprKind(
receiverExpr, localsIn)) : "";`) is deleted, along with the
`auditReceiverTypeAgainstFallbackExpr` observational diff-audit harness
this round's predecessor (`eba4a4208`) wired at this call site - diffing
`resolveReceiverTypeFromFallbackExpr` against itself post-migration is
meaningless, the same retirement reasoning RT2 and RT3b's own migration
rounds used. The now-unused `<cassert>`/`<iostream>` includes and the
`primec/support/ReceiverElementFamilyClassifier.h` include (needed only by
the retired harness's `isReceiverTargetDiffAuditEnabled()` check) were
removed too, mirroring RT3b's migration commit exactly.

With this change, **all three of `resolveMethodReceiverTarget`'s
branches - RT3a/RT2 (`Name`-kind), RT3b (`Call`-kind), and RT3c (the
fallback) - are single-production-path**, each delegating to its own
`CanonicalReceiverType`-producing sibling function above it in this file.
This is the last piece of `resolveMethodReceiverTarget` itself; the
function is now fully migrated onto the new module.

### Re-reading `resolveMethodReceiverTarget` fresh: is it now a thin dispatcher?

Mostly, but not entirely. The `Call`-kind and fallback branches are now
pure delegation: construct a `CanonicalReceiverType`, call the sibling
function, copy its fields into the legacy out-parameters, `return true`.
Nothing else happens in either branch.

The `Name`-kind branch (RT3a) is different and is the one place genuine
wrapping logic still lives directly in `resolveMethodReceiverTarget`
itself, beyond the three branches' own bodies: after calling
`resolveMethodReceiverTypeFromNameExpr` (RT2's own thin wrapper around
`resolveReceiverType`), if that call fails the branch falls through to a
second, independent resolution attempt -
`resolveStructTypePathFromName(receiverExpr.name, receiverExpr.namespacePrefix,
importAliases, structNames)` - and only returns `false` (the function's
*only* failure exit) if that second attempt also comes up empty. This
`resolveStructTypePathFromName` fallback is call-site logic, not part of
RT2's `resolveReceiverType` question ("what type does this local have"),
and it was never proposed for delegation into `CanonicalReceiverType` by
any prior round - RT2's own migration left it exactly where it already
was. It is a legitimate candidate for a future round to look at (whether
it belongs inside `resolveReceiverType` itself, as a second-chance
struct-path lookup when the local-based classification fails, or should
stay call-site logic as-is) but is **not** acted on this round - noted
here only as an observation per this round's task.

Net: `resolveMethodReceiverTarget` is now a thin dispatcher for two of
its three branches (`Call` and fallback), with one small piece of
genuine, never-yet-migrated wrapping logic remaining around the
`Name`-kind branch's failure path.

### Verification

Fresh baseline (`git stash -u` back to the clean `eba4a4208` tree, clean
release rebuild of `primec_ir_lib` and all three suites with
`-Wall -Wextra -Wpedantic -Werror`, no warnings, all three suites run
foreground):

| suite | test cases | failed | assertions | failed |
|---|---|---|---|---|
| semantics | 2767 | 1 | 13343 | 2 |
| backend_ir | 1646 | 46 | 16428 | 137 |
| compile_run | 2679 | 5 | 15278 | 8 |

Identical failed-test-case counts to every prior round of this effort
(the one `soa reads` semantics flake, the same 46 `backend_ir` names, the
same 5 `map`-conformance `compile_run` names).

`git stash pop` restored the migration; rebuilt clean (no warnings/
errors, confirming `resolveReceiverTypeFromFallbackExpr` and
`CanonicalReceiverType` are both real, exercised production code paths
now - not dead functions). The full three-suite battery was then run
twice more (foreground only, per this doc's safety discipline). Failing
test-case **names** (not just counts) were diffed pairwise across all
three runs (baseline vs. run 1, baseline vs. run 2, run 1 vs. run 2) for
all three suites - all nine comparisons came back byte-identical (`diff`
empty). Every run's assertion counts also matched exactly across all
three runs for all three suites (16428/137 backend_ir, 15278/8
compile_run, 13343/2 semantics) - no run-to-run wobble this round,
unlike the small `compile_run` assertion-count variance noted in prior
rounds' verification sections (pre-existing, unrelated to this change).
Confirmed via `pgrep -fc '^\./PrimeStruct_<suite>_tests$'` (anchored full
binary path) that no concurrent test-suite instance ran at any point.

### What remains after this round

`resolveMethodReceiverTarget` itself has no un-migrated branch logic
left. Two items remain open for TODO-5294 overall: G7's own `Call`-kind
sub-cascade in `IrLowererSetupTypeMethodCallResolution.cpp` (re-confirmed
above as next in line, a genuine `resolveReceiverType`-shaped candidate)
and monomorphization's F3 producer. See `docs/todo.md` for current
per-item status. The `Name`-kind branch's `resolveStructTypePathFromName`
fallback (see above) is a smaller, non-blocking observation for whoever
picks up G7 or does a future pass over this file, not a scoped item.

## Step 1c, F3 harness round: G7 re-confirmed as no new work; monomorphization's F3 producer implemented for Name/literal receivers (2026-09-10)

### Re-checking G7 fresh: it already sits on the new module

Before touching monomorphization, this round re-read Row G's G7
(`IrLowererSetupTypeMethodCallResolution.cpp:897-913`) against the state
`resolveMethodReceiverTarget` was left in by the immediately-preceding
round, rather than trusting last round's own "why G7 was not started"
note to still hold unchanged. It does not, in G7's favor: G7's own body is
nothing more than

```cpp
if (!resolveMethodReceiverTarget(*receiver, localsIn, explicitMethodPath,
                                 semanticAwareImportAliases, structNames,
                                 inferExprKind, resolveExprPath, typeName,
                                 resolvedTypePath, errorOut, semanticProgram,
                                 semanticIndexPtr)) {
  if (allowBuiltinFallback) { errorOut = priorError; }
  return nullptr;
}
```

a single call plus a failure-path `errorOut` restore - no cascade logic
of G7's own to migrate. And `resolveMethodReceiverTarget` is now, as of
this document's own immediately-preceding "RT3c real migration" section,
a fully single-production-path thin dispatcher onto the
`resolveReceiverType`/`resolveReceiverTypeFromCallExpr`/
`resolveReceiverTypeFromFallbackExpr` family for all three of its
branches. G7 therefore already sits entirely on the new module,
transitively, purely as a side effect of RT3c's migration landing last
round - there is no independent G7 cascade left to characterize or
migrate. This supersedes every prior round's framing of G7 as "next in
line" waiting for its own implementation round: it needed none. Per the
task's own step 7 fallback ("if G7 turns out not to fit cleanly...
instead start on monomorphization's F3 producer"), this counts as exactly
that case - not a bad structural fit, but zero remaining work - and this
round proceeded to F3.

### F3 implemented for Name-kind and primitive-literal-kind receivers only

Added an independent `resolveReceiverType` (anonymous namespace,
`TemplateMonomorphMethodTargets.cpp`, immediately above
`resolveMethodCallTemplateTarget`) reproducing F3-N1/N2 (`Name`-kind
receiver, via a from-scratch reimplementation of
`qualifyImportedCollectionTypeText`/`bindingTypeText`/
`isBorrowedSoaReceiverType`/`unwrapImportedCollectionReceiverType`,
deliberately not sharing code with the lambdas
`resolveMethodCallTemplateTarget` defines locally, matching every prior
`resolveReceiverType` producer's own independent-reimplementation
precedent) and F3-L/B/Fl/S (the four primitive-literal receiver kinds).
Fills `CanonicalReceiverType::collectionBaseName` from the unwrapped type
text, `wrappedBaseTypeName` from the raw (possibly-still-wrapped)
qualified type text - faithfully as a second, independent field, matching
the design doc's own field-list note that F3's wrapped-type text is not
redundant with `collectionBaseName` the way ir_lowerer's is - `isBorrowed`
from the same borrowed-SOA check F3 itself runs, and derives `isWrapped`
by checking whether `wrappedBaseTypeName`'s own base (post-normalization)
is `Reference`/`Pointer`. No `CanonicalReceiverType` struct changes were
needed; every field this slice fills already existed from the prior
round's scoping work.

### Why Call-kind receivers (F3-C1/C2/C3) are out of scope this round

Traced concretely rather than assumed, because this is the first F3 slice
and the first time this document's `resolveReceiverType` precedent meets
a stage whose underlying inference is not obviously pure. F3-C1
(`inferBindingTypeForMonomorph`), F3-C2's fallback
(`inferExprTypeTextForTemplatedVectorFallback`), and F3-C3c's fallback
(`inferDefinitionReturnBindingForTemplatedFallback`) all take a
non-`const Context&` and, transitively (via
`inferCallBindingTypeForMonomorph` → `inferImplicitTemplateArgs`), mutate
two ctx-scoped fields on every cached-implicit-template-arg-fact hit:
`Context::implicitTemplateArgInferenceFactHitsForTesting` (incremented
unconditionally on a cache hit) and `implicitTemplateArgFactsForTesting`
(appended to, gated on the separate
`collectImplicitTemplateArgFactsForTesting` flag) - see
`TemplateMonomorphContext.h` and their only readers,
`TemplateMonomorph.cpp`'s own test-facing hit-count/fact-list reporting
functions. These are real, test-visible side effects of *calling* the
inference helpers at all, independent of whatever type answer they
return.

RT2/RT3b/RT3c's own underlying inference (plain `LocalInfo` field reads
and `Expr::Kind` switches) has no such side channel, which is exactly
what made re-invoking it a second time, from a purely-observational
diff-audit, safe in every one of this document's prior harness rounds.
Re-invoking F3's Call-kind inference a second time from an audit would
not be safe in the same way: it would silently double-increment/double-
append these counters for any call whose Call-kind receiver happens to
hit the implicit-template-arg-fact cache, corrupting them for any test
that asserts on the resulting hit count or fact list - a genuine
violation of this round's own "harness only observes, production stays
behaviorally unchanged" discipline, found by tracing the actual
dependency functions rather than guessed at.

Call-kind receivers are therefore explicitly out of scope this round:
`resolveReceiverType` returns `false` for them without attempting any
inference (and without calling any of the three side-effecting helpers
above), and the diff-audit call site skips the comparison entirely
whenever the receiver is `Call`-kind (or any kind besides the five
covered) rather than exercising a path this function does not implement.
F3-C3a (the receiver-is-a-struct-constructor-call short-circuit) stays
permanently out of scope regardless, unaffected by this finding - that
was already settled as an irreconcilable case in an earlier round.

A future round wanting to cover F3-C1/C2/C3b/c/d should look at running
the side-effecting helpers against a throwaway deep copy of `Context`
(so any mutation lands on the copy, never on the real `ctx` the audit was
handed) rather than `ctx` itself, before extending this function's
coverage - not yet attempted, since `Context` is large enough
(`sourceDefs`/`templateDefs`/etc.) that a deep copy per audited call site
is a real performance question of its own, left open rather than
guessed at.

### Wiring: a direct audit call, not a scope-exit guard

Mirrors RT3c's own "single exit point" pattern rather than RT3b's RAII
guard: `resolveMethodCallTemplateTarget`'s F3 cascade has exactly one
place execution reaches after finishing (immediately before
`resolveIndexedArgsPackMapMethodTarget` is consulted, which can otherwise
discard `typeName`/`wrappedReceiverTypeName`/`isBorrowedSoaReceiver`
without ever using them), so a single direct call to
`auditReceiverTypeAgainstTemplateMonomorphExpr` placed at that one point
suffices - no early-return path in this function bypasses it before the
locals it reads are fully set. Gated on the existing
`isReceiverTargetDiffAuditEnabled()`/`PRIMESTRUCT_RECEIVER_TARGET_DIFF_AUDIT`
check, same as every prior round's harness. `#include
"primec/support/CanonicalReceiverType.h"` was added;
`<cassert>`/`<iostream>`/`primec/support/ReceiverElementFamilyClassifier.h`
were already included in this file (the latter is where
`isReceiverTargetDiffAuditEnabled` itself is declared), so no other
include changes were needed.

### Verification

Fresh baseline (`git stash -u` back to the clean `62958f772` tree,
rebuild of `primec_frontend_lib` and all three suites, no warnings, all
three suites run foreground):

| suite | test cases | failed | assertions | failed |
|---|---|---|---|---|
| semantics | 2767 | 1 | 13343 | 2 |
| backend_ir | 1646 | 46 | 16428 | 137 |
| compile_run | 2679 | 5 | 15278 | 8 |

Identical to every prior round of this effort. `git stash pop` restored
the harness; rebuilt clean (no warnings/errors). Ran the full three-suite
battery twice more with `PRIMESTRUCT_RECEIVER_TARGET_DIFF_AUDIT=1` set:
zero `receiver-target-diff-audit` mismatches in either run (this
function's own assert would have aborted the process on any divergence),
identical counts to baseline both times, and failing-test-case *names*
diffed pairwise (baseline vs run1, baseline vs run2, run1 vs run2, for
all three suites) - all nine comparisons byte-identical. Ran the full
battery twice more again with the env var unset, with the same pairwise
name-diff discipline - also byte-identical throughout, confirming the
harness has zero effect on production behavior either way. Confirmed via
`pgrep -fc '^\./PrimeStruct_<suite>_tests$'` (anchored full binary path)
that no concurrent test-suite instance ran at any point.

`compile_run`'s full suite consistently takes longer than a single
foreground Bash call's ~590s budget in this environment. One attempt to
force it under that budget with an external `timeout 580` wrapper
produced a misleading partial result (doctest printed a summary showing
1405 "skipped" cases after being killed mid-run, which is really
"interrupted", not "skipped") - abandoned in favor of issuing the run
without an external timeout cap and letting this harness's own
auto-background-and-notify mechanism take over, per this round's safety
discipline. Every `compile_run` number cited above and below reflects a
run that was allowed to finish on its own.

### What remains after this round

Monomorphization's F3 now has a real (harnessed-only, not yet migrated)
`resolveReceiverType` for its Name-kind and primitive-literal-kind
receivers. Call-kind receivers (F3-C1/C2/C3b/c/d) remain unimplemented
here, for the ctx-mutation reason documented above - a future round
should resolve that before extending coverage, most likely via a
throwaway `Context` copy. F3-C3a stays permanently outside
`resolveReceiverType`/`CanonicalReceiverType` per the earlier
irreconcilable-case finding, unaffected by this round. No call site
anywhere (monomorphization or `ir_lowerer`) has been migrated onto the
new module yet - this round, like every harness-only round before it,
changes no production behavior. G7 needs no separate implementation
round of its own; it is already fully covered by `resolveMethodReceiverTarget`'s
existing migration.

## Step 1c, F3 real migration (PARTIAL): Name/literal receivers migrated, Call-kind stays unmigrated and blocked (2026-09-10/11)

### What changed

`resolveMethodCallTemplateTarget`'s F3 cascade (`TemplateMonomorphMethodTargets.cpp`)
now calls the harnessed `resolveReceiverType` directly for Name-kind and
primitive-literal-kind receivers (F3-N1/N2, F3-L/B/Fl/S) instead of
running its own inline re-derivation for those five kinds:

```cpp
if (receiver.kind == Expr::Kind::Name || receiver.kind == Expr::Kind::Literal ||
    receiver.kind == Expr::Kind::BoolLiteral || receiver.kind == Expr::Kind::FloatLiteral ||
    receiver.kind == Expr::Kind::StringLiteral) {
  CanonicalReceiverType canonical;
  resolveReceiverType(receiver, locals, ctx, canonical);
  wrappedReceiverTypeName = canonical.wrappedBaseTypeName;
  isBorrowedSoaReceiver = canonical.isBorrowed;
  typeName = canonical.collectionBaseName;
} else if (receiver.kind == Expr::Kind::Call) {
  // ... completely untouched inline cascade (F3-C1/C2/C3/C3b/c/d) ...
}
```

This is the same `CanonicalReceiverType` → legacy-local translation shape
RT2/RT3b/RT3c's own real migrations used - `collectionBaseName` is the
cascade's `typeName`, `wrappedBaseTypeName` is `wrappedReceiverTypeName`,
`isBorrowed` is `isBorrowedSoaReceiver`; `isWrapped` has no legacy F3
consumer (matching the prior round's own field-list note) and is simply
not read here, same as every other real migration in this document. For
the F3-N2 case (unbound `Name` receiver), `resolveReceiverType` returns
`false` with `out` left at its default-constructed
`CanonicalReceiverType{}` - all three translated fields come out empty/
false, exactly matching the legacy branch's own behavior of leaving
`typeName`/`wrappedReceiverTypeName`/`isBorrowedSoaReceiver` untouched
when `locals.find(receiver.name)` misses.

The Call-kind branch (F3-C1/C2/C3/C3b/c/d) is completely untouched -
same inline `inferBindingTypeForMonomorph`/
`inferExprTypeTextForTemplatedVectorFallback`/struct-and-`return<T>`-
transform/`inferDefinitionReturnBindingForTemplatedFallback`/
`getBuiltinCollectionName` cascade as before this round, still not
calling `resolveReceiverType` at all, per the ctx-mutation blocker the
harness round found.

Deleted the now-dead harness scaffolding for the migrated kinds only:
`auditReceiverTypeAgainstTemplateMonomorphExpr` (the diff-audit helper)
and its call site inside `resolveMethodCallTemplateTarget`, plus the
`<cassert>`/`<iostream>` includes that existed solely to support it (no
other code in this file used `assert`/`std::cerr`/`std::cout` - grepped
to confirm before removing). `resolveReceiverType` itself is unchanged
from the harness round - it is now a production dependency instead of an
audit-only one, but its body was already correct (that was the whole
point of harnessing it first). Grepped the repo for
`auditReceiverTypeAgainstTemplateMonomorphExpr` post-deletion: the only
remaining references are in this document and `docs/todo.md`, confirming
nothing else depended on it.

Net line count: the ~15-line inline Name/literal branch shrank to a
~10-line translation shim, and the ~28-line audit function plus its
~7-line call site were deleted outright - roughly 40 lines of now-dead
cascade/harness code removed from this file, with no new lines added
beyond the shim itself.

### Verification

Fresh baseline (`git stash -u` back to the clean `79a6cc6a2` tree,
rebuild of `primec_frontend_lib` and all three suites, no warnings, all
three suites run foreground):

| suite | test cases | failed | assertions | failed |
|---|---|---|---|---|
| semantics | 2767 | 1 | 13343 | 2 |
| backend_ir | 1646 | 46 | 16428 | 137 |
| compile_run | 2679 | 5 | 15278 | 8 |

Identical to every prior round. `git stash pop` restored the migration;
rebuilt clean (no warnings/errors, `resolveReceiverType` compiling as a
real dependency of `resolveMethodCallTemplateTarget` now rather than an
audit-only helper). Ran the full three-suite battery twice more (no env
var involved this time - there is no more diff-audit to gate, the
migration is unconditional): identical counts to baseline both times,
and failing-test-case *names* diffed pairwise (baseline vs run1,
baseline vs run2, run1 vs run2, for all three suites) - all nine
comparisons byte-identical. Confirmed via `pgrep -fc
'^\./PrimeStruct_<suite>_tests$'` (anchored full binary path) that no
concurrent test-suite instance ran at any point. `compile_run`'s full
suite again exceeded a single foreground Bash call's ~590s budget on all
three runs (baseline, run1, run2); each was let finish via the harness's
own auto-background-and-notify mechanism from an already-issued
foreground/blocking call, never via a background-launched or polled
process.

### Call-kind side-effect blocker: characterized further, not fixed this round

Per this round's own optional step, the F3-C1/C2/C3 ctx-mutation blocker
the harness round found was traced to its actual mutation sites rather
than left at the prior round's higher-level description. Grepped
`TemplateMonomorphImplicitTemplateInference.cpp`,
`TemplateMonomorphBindingCallInference.cpp`, and
`TemplateMonomorphFallbackTypeInference.cpp` (the full transitive closure
of `inferBindingTypeForMonomorph`/`inferCallBindingTypeForMonomorph`/
`inferImplicitTemplateArgs`/`inferExprTypeTextForTemplatedVectorFallback`/
`inferDefinitionReturnBindingForTemplatedFallback`) for every `ctx.<field>`
write. Exactly three exist:

1. `++ctx.implicitTemplateArgInferenceFactHitsForTesting` - unconditional,
   on every cached-fact hit. **Non-idempotent**: a second (audit) call
   hitting the same cache entry increments this a second time. This is
   the counter the harness round's finding named.
2. `ctx.implicitTemplateArgFactsForTesting.push_back(...)` - gated on
   `ctx.collectImplicitTemplateArgFactsForTesting`. **Non-idempotent**
   for the same reason, when that gate is on.
3. `ctx.implicitTemplateArgInferenceFacts[key] = ImplicitTemplateArg-
   InferenceFact{outArgs}` - the real (non-"ForTesting") inference-fact
   cache. **Idempotent** on a same-key second call: `inferImplicitTemplateArgs`
   is a pure function of its inputs, so re-deriving and reassigning the
   same key produces the identical value; a second write is a no-op in
   effect, not a corruption.

A fourth site initially looked concerning -
`inferDefinitionReturnBindingForTemplatedFallback`'s
`ctx.returnInferenceStack.insert(def.fullPath)` recursion guard - but it
is provably safe to re-invoke: it is wrapped in a scoped RAII guard
(`InferenceScopeGuard`, `~InferenceScopeGuard() { stack.erase(fullPath); }`)
that unconditionally erases its own entry before the function returns on
every exit path, so a second sequential call (the audit call, strictly
after the production call completes - never concurrent, never
re-entrant) observes `returnInferenceStack` exactly as the production
call left it. No template-instantiation call, and no
`sourceDefs`/`outputDefs`/`specializationCache`/`helperOverloads` write
of any kind, was found anywhere in this inference chain.

**Conclusion: the blocker is narrower than "mutates Context broadly" -
it is exactly the two `...ForTesting` counter fields, both of which are
plain scalar/vector state with no relationship to the rest of `Context`.**
A full `Context` deep copy (this document's own prior "future round"
suggestion) would be sufficient but more than the blocker requires - a
much cheaper fix is to snapshot both counter fields immediately before
the audit's second invocation of the side-effecting helpers and restore
them immediately after (`ctx.implicitTemplateArgInferenceFactHitsForTesting`
reset to its saved value; `ctx.implicitTemplateArgFactsForTesting`
`resize()`d back to its saved length) - no `Context` copy, no
`sourceDefs`/`helperOverloads`/etc. duplication, and no risk to the
(already-established-safe) cache and recursion-guard mutations, which
are left to run and settle normally.

This is a genuinely clean, low-risk approach and a future round could
implement it - but doing so was **not attempted this round**, per this
round's own explicit "skipping is a perfectly good outcome" allowance:
harnessing F3-C1/C2/C3/C3b/c/d for real still requires writing a
from-scratch `resolveReceiverType` reimplementation of that entire
cascade (including its recursive `resolveMethodCallTemplateTarget`/
`resolveCalleePath` resolution step and the `ctx.sourceDefs`-lookup
struct-definition/`return<T>`-transform paths), wiring a new audit call
site around the counter-snapshot, and running this document's full
fresh-baseline/byte-identical-name-diff verification discipline again -
real, separately-sized work better done as its own round than folded
into this one alongside the Name/literal migration. F3-C3a remains
permanently out of scope regardless, unaffected by any of this.

### What remains after this round

F3's Name-kind and primitive-literal-kind receivers (F3-N1/N2,
F3-L/B/Fl/S) are now for real on `resolveReceiverType`, joining RT2 (ir_lowerer),
RT3b/RT3c (via `resolveMethodReceiverTarget`), and G7 (transitively) as
fully-migrated call sites. F3's Call-kind receivers (F3-C1/C2/C3b/c/d)
remain on their original inline cascade, unharnessed and unmigrated - the
counter-snapshot approach characterized above is a concrete, sized next
step for a future round, not yet implemented. F3-C3a stays permanently
outside `resolveReceiverType`/`CanonicalReceiverType`. No other call site
in this document's scope changed this round.

## Step 1c, F3 Call-kind harness round: `resolveReceiverTypeFromCallExprForTemplateMonomorph`, counter-restoration verified, zero-divergence achieved, NOT migrated (2026-09-11)

### What changed

Implemented the counter-snapshot approach characterized (but not
attempted) in the round above. Two new functions were added to
`TemplateMonomorphMethodTargets.cpp`, both file-local (anonymous
namespace):

- `resolveReceiverTypeFromCallExprForTemplateMonomorph` - a thin wrapper
  that re-invokes the same three production inference helpers F3's own
  inline Call-kind cascade already calls
  (`inferBindingTypeForMonomorph`, `inferExprTypeTextForTemplatedVectorFallback`,
  `inferDefinitionReturnBindingForTemplatedFallback`), producing a
  `CanonicalReceiverType`. Deliberately not an independent
  reimplementation - those helpers are already-delegated production
  logic, not inline algorithm this file could faithfully re-derive from
  scratch, matching the precedent set by `ir_lowerer`'s own
  `resolveReceiverTypeFromCallExpr` (RT3b's producer). F3-C3a (the
  struct-constructor-call short circuit) is a defensive no-op branch
  here, not a live path: production's own inline cascade already returns
  early via its own C3a check before the audit call site (below) is ever
  reached.
- `auditReceiverTypeAgainstTemplateMonomorphCallExpr` - gated on
  `PRIMESTRUCT_RECEIVER_TARGET_DIFF_AUDIT`, wired into
  `resolveMethodCallTemplateTarget` right after the Call-kind cascade
  settles (mirroring the removed Name/literal harness's own "single exit
  point" placement) and before `resolveIndexedArgsPackMapMethodTarget()`'s
  own check. It snapshots `ctx.implicitTemplateArgInferenceFactHitsForTesting`
  (scalar) and `ctx.implicitTemplateArgFactsForTesting.size()` (vector
  length) immediately before calling the wrapper above, then restores
  both immediately after - unconditionally, whether the audit path hit a
  cache hit, a miss, or recursed - before comparing the audit's
  `CanonicalReceiverType` against production's already-computed
  `typeName`/`wrappedReceiverTypeName`/`isBorrowedSoaReceiver`. A
  mismatch (or a success/failure disagreement) trips an `assert` and logs
  to `std::cerr`; not wired into any behavior change.

### Verifying the snapshot/restore is actually correct (not just "looks right")

Before trusting this, re-read every mutation site of the two
`...ForTesting` fields across the whole codebase (not just this file):
both fields are written in exactly three places, all in
`TemplateMonomorphImplicitTemplateInference.cpp` -
`ctx.implicitTemplateArgInferenceFactHitsForTesting` is only ever
`++`-incremented (one site, on a cache hit), and
`ctx.implicitTemplateArgFactsForTesting` is only ever `push_back`-ed to
(two sites, both gated on `ctx.collectImplicitTemplateArgFactsForTesting`).
Nothing anywhere erases, reorders, or otherwise mutates existing elements
of the vector. That makes the restore approach correct on both counts: a
plain scalar save/restore is sufficient for the hit counter, and
`resize()`-ing the vector back down to its saved length is equivalent to
a full restore precisely because every write is an append - no
reordering or splicing exists that a `resize()` could get wrong. The
third ctx-scoped write this file's audit touches indirectly
(`ctx.implicitTemplateArgInferenceFacts`, the real non-"ForTesting"
cache) is a plain `map[key] = value` assignment keyed and valued
deterministically from the same inputs; a second write for the same
receiver during the audit's second invocation writes the identical value
back, so leaving it unrestored is correct (and restoring it would be
actively wrong, capable of reverting a legitimate first population). The
recursion guard (`ctx.returnInferenceStack`) is RAII-scoped
(`InferenceScopeGuard`, `~InferenceScopeGuard() { stack.erase(fullPath); }`)
and erases on every exit path including early returns, so it self-cleans
with no snapshot needed.

### Verifying counter-restoration empirically, not just by code inspection

Static reasoning above is necessary but not sufficient by itself, so it
was checked directly against running tests:

- `tests/unit/semantics/type_resolution/test_semantics_type_resolution_graph_snapshots_require_predicates_facts_ct_if.cpp`
  and `.../test_semantics_type_resolution_graph_snapshots_targets_semantic_product_soa.cpp`
  both assert on the exact contents of `implicitTemplateArgFactsForTesting`
  (via `collectImplicitTemplateArgResolutionFactsForTesting`, exact
  `targetPath`/`scopePath`/`callName`/`inferredArgsText` string matches)
  and on `implicitTemplateArgInferenceFactHitsForTesting` (via
  `collectImplicitTemplateArgFactConsumptionMetricsForTesting`,
  `hitCount > 0u`). These are part of `PrimeStruct_semantics_tests`,
  which was run under the full battery below in both configurations
  (`PRIMESTRUCT_RECEIVER_TARGET_DIFF_AUDIT` set and unset) and produced
  byte-identical pass/fail results test-by-test in both cases, including
  a pre-existing, unrelated failure in the SOA-borrowed-receiver test
  case in the same file reproducing at the identical assertion line with
  the identical logged values in both configurations.
- Fresh baseline (`git stash -u` back to clean `93ed1c94a`, rebuilt
  release) vs. the harness applied, across all three suites
  (`PrimeStruct_semantics_tests`, `PrimeStruct_backend_ir_tests`,
  `PrimeStruct_compile_run_tests`), run foreground:
  - With `PRIMESTRUCT_RECEIVER_TARGET_DIFF_AUDIT=1`: zero `MISMATCH`/
    assert output across all three suites; failing-test-*name* sets
    byte-identical to baseline (semantics: 1/2767 failing, same case;
    backend_ir: 46/1646 failing, same 46 cases; compile_run: 0/­all
    passing).
  - With the env var unset (default, harness compiled in but inert): two
    full foreground reruns per suite, both byte-identical in failing
    test *names* (not just counts) to the fresh baseline, confirming the
    harness's mere presence changes nothing about default behavior.

This is the actual property the whole exercise exists to guarantee - not
"the audit's answer usually agrees with production" but "invoking the
audit path a second time leaves `ctx`'s two non-idempotent test-visible
counters exactly as production's own first invocation left them" - and
it was verified directly via tests that assert on those counters' exact
values/contents, not merely inferred from the classification-agreement
result.

### What remains after this round

The Call-kind branch (F3-C1/C2/C3b/c/d) is now harnessed
observationally but **still not migrated to production** - per the
two-round discipline every other RT2/RT3b/RT3c/F3-Name-literal migration
in this document followed, `resolveMethodCallTemplateTarget`'s own
inline cascade remains the sole production path for Call-kind receivers;
a future round can migrate it onto `resolveReceiverType` now that this
round's zero-divergence and counter-restoration verification give it a
safe foundation to build on. F3-C3a stays permanently outside
`resolveReceiverType`/`CanonicalReceiverType`, unaffected by any of this.

## Step 1c, F3 Call-kind real migration: F3 now FULLY migrated end-to-end (2026-09-11)

### What changed

Migrated the last unmigrated slice of F3 - Call-kind receivers
(F3-C1/C2/C3b/c/d) - onto `resolveReceiverTypeFromCallExprForTemplateMonomorph`
(the previous round's harness producer, already proven zero-divergence and
counter-restoration-safe). `resolveMethodCallTemplateTarget`'s own old
inline Call-kind cascade is deleted, along with the now-pointless
`auditReceiverTypeAgainstTemplateMonomorphCallExpr` (diffing a function
against itself post-migration is meaningless, matching every prior
migration's own retirement pattern in this document) and four lambdas in
the outer function (`qualifyImportedCollectionTypeText`/`bindingTypeText`/
`isBorrowedSoaReceiverType`/`unwrapImportedCollectionReceiverType`) that
were only ever called by the deleted cascade and had no other caller. Net
change in `TemplateMonomorphMethodTargets.cpp`: 344 lines removed, 178
added (many of them expanded/updated comments), for a net -166 lines.

### F3-C3a: preserved exactly, via one new out-parameter

F3-C3a (the receiver-is-a-struct-constructor-call short circuit) is a
resolution question ("what definition path"), not an inference one ("what
type"), and per every prior round's finding it stays permanently outside
`CanonicalReceiverType`'s scope. The harness-round producer already
encoded this correctly for audit purposes - it returns `false` without
filling `out` when C3a's shape is detected - but that was insufficient for
a *production* call site: the caller needs to actually run C3a's own
short-circuit (`pathOut = resolved + "/" + methodName; return true;`),
which requires the already-resolved callee path (`resolved`), not just a
bare `false`.

The naive fix - have the call site independently re-resolve `resolved`
itself to check for C3a before calling the producer - would have been
wrong: resolving the callee path for a method-call receiver can
recursively invoke `resolveMethodCallTemplateTarget`, which (after this
same migration) now calls the side-effecting inference helpers
(`inferBindingTypeForMonomorph`/`inferExprTypeTextForTemplatedVectorFallback`/
`inferDefinitionReturnBindingForTemplatedFallback`) for its own Call-kind
receiver. Resolving `resolved` twice - once in a pre-check, once again
inside the producer - would have invoked those helpers twice for any
nested method-call receiver, double-counting the two non-idempotent
`...ForTesting` counters the previous harness round's whole
snapshot/restore exercise existed to protect.

The fix instead: `resolveReceiverTypeFromCallExprForTemplateMonomorph`
gained a `std::string &structConstructorReceiverPathOut` out-parameter,
cleared at entry and filled with `resolved` at the exact point the
function used to just `return false` for the C3a shape. The call site
now:

```cpp
CanonicalReceiverType canonical;
std::string structConstructorReceiverPath;
resolveReceiverTypeFromCallExprForTemplateMonomorph(
    receiver, locals, const_cast<Context &>(ctx), canonical, structConstructorReceiverPath);
if (!structConstructorReceiverPath.empty()) {
  pathOut = selectHelperOverloadPath(
      expr, structConstructorReceiverPath + "/" + methodName, ctx);
  return true;
}
wrappedReceiverTypeName = canonical.wrappedBaseTypeName;
isBorrowedSoaReceiver = canonical.isBorrowed;
typeName = canonical.collectionBaseName;
```

This calls the helpers exactly once regardless of which path is taken -
matching the old inline cascade's own invocation count exactly, and
byte-identical to the old cascade's own C3a branch
(`pathOut = selectHelperOverloadPath(expr, resolved + "/" + methodName, ctx); return true;`)
when it fires.

### Verification

Fresh baseline: this round's starting commit (`73f141852`, the prior
harness round's own commit) was already the clean checked-out state - no
`git stash` round-trip was needed since no edits existed yet. Built
release, ran the full 3-suite battery foreground (compile_run's run
exceeded a single foreground call's ~590s budget in this environment, so
it was launched detached-and-blocking-`wait`ed on its own PID within one
logical foreground operation, per this document's established pattern for
that suite):

| suite | test cases | failed |
|---|---|---|
| semantics | 2767 | 1 |
| backend_ir | 1646 | 46 |
| compile_run | 2679 | 5 |

Identical counts to every prior round's documented baseline. Migration
applied, rebuilt clean (`-Wall -Wextra -Werror`, no warnings), ran the
same battery **twice more** (foreground only, same per-suite
methodology). All three runs' failing-test-case **names** (not just
counts) were extracted via proper JUnit-XML parsing (`xml.etree.ElementTree`,
not a hand-rolled regex - an earlier regex-based attempt this round
produced spurious results on self-closing `<testcase .../>` tags and was
discarded in favor of real XML parsing before trusting any comparison)
and compared pairwise: baseline == run 1 == run 2, byte-for-byte, across
all three suites (semantics: the same 1 case; backend_ir: the same 46
cases; compile_run: the same 5 cases).

The two counter-asserting tests were checked explicitly by name in all
three result files, not inferred from the aggregate pass/fail counts:
`test_semantics_type_resolution_graph_snapshots_targets_semantic_product_soa.cpp`'s
"implicit template-arg graph facts are consumed by inference cache"
(asserts `hitCount > 0u`) and
`test_semantics_type_resolution_graph_snapshots_require_predicates_facts_ct_if.cpp`'s
"implicit template-arg graph facts publish inferred argument facts" and
"...publish helper-routing scope" (both assert exact fact-vector
contents) all showed PASS/PASS/PASS in baseline, run 1, and run 2 alike.
This is the property this whole two-round exercise (harness, then real
migration) existed to guarantee: with the audit's second invocation gone
entirely, production's single invocation of the side-effecting helpers
leaves the two `...ForTesting` counters in exactly the state they were
always in - confirmed directly against the tests that assert on those
counters' exact values, not merely inferred from equal failure counts.

### F3 is now fully migrated - what remains for this whole module

F3's entire receiver-type-inference cascade - F3-N1/N2 (Name-kind),
F3-L/B/Fl/S (primitive-literal-kind), and now F3-C1/C2/C3b/c/d
(Call-kind) - runs on `resolveReceiverType`/
`resolveReceiverTypeFromCallExprForTemplateMonomorph` for real, mirroring
`resolveMethodReceiverTarget`'s (RT2/RT3a/RT3b/RT3c/G7) completion in
`ir_lowerer` two rounds ago. F3-C3a stays permanently outside
`resolveReceiverType`/`CanonicalReceiverType`'s scope, by design, in both
stages that have one (F3-C3a here; there is no `ir_lowerer` analogue,
since `resolveMethodReceiverTarget` never had a resolution-not-inference
carve-out of its own).

This closes out every site the Step 1c Scoping round (2026-09-10)
originally named as needing `CanonicalReceiverType`/`resolveReceiverType`:
F3, RT2, and RT3/G7 are now **all** migrated. No further
receiver-type-inference site (as opposed to receiver-*family*
classification, `classifyReceiverElementFamilyJoint`'s separate and still
partially open concern - Row F still has 12/17 branches unmigrated
(F0-F6/F8, F10, F14-F16) plus F12's deferred real migration, tracked
under TODO-5294's own ongoing Step 0/Step 2 work, not this "new module")
is currently known to exist in any of the three stages this document
covers. This new module's own implementation work - the
inference/family-classification split characterized in the Step 1c
Scoping round - is therefore essentially complete: what remains is not
a known open inference site but a final full-scope review pass to
confirm no other receiver-type-inference mechanism was missed by the
original characterization, before this sub-track of TODO-5294 could be
considered fully closed.

## Closing Summary: TODO-5294 review and closure (2026-09-11)

This round performed the final-review pass the previous section called
for: a fresh, adversarial re-read of this entire document front to back,
cross-checking every "fully migrated" claim against current source
(not doc prose), a repeat of the full 3-suite battery against the
cumulative HEAD state, and a fresh search for any receiver-type-inference
logic this multi-week effort might have missed. Conclusion: **both
consolidation tracks are at real, defensible stopping points.** No new
migration work was done this round; one small piece of genuinely dead
code this review itself surfaced was removed (see below), matching the
document's own established F14 precedent for "delete proven-dead code
outright, do not just document it."

### Track 1 final tally: `classifyReceiverElementFamilyJoint` (receiver-family classification)

Migrated for real (verified this round via `grep` against current
source - every call site below now calls
`classifyReceiverElementFamilyJoint(...)`, and the old inline cascades it
replaced are gone, not merely bypassed):

- **2 semantics call sites**: `resolveArgsPackElementMethodTarget`
  (`SemanticsValidatorMethodTargetArgsPackResolvers.cpp`) and
  `resolveMethodTarget`'s inline indexed-args-pack-element cascade
  (`SemanticsValidatorExprMethodTargetResolution.cpp`).
- **5 monomorphization branches** (all in
  `TemplateMonomorphMethodTargets.cpp`'s `resolveMethodCallTemplateTarget`):
  F7 (File-family), F9 (primitive), F11 (FileError sub-case), F12
  (generic-Soa receiver), F13/F13b/F13c (collection-family fallback).
- **F14 deleted as proven-dead code** (not migrated - proven
  unreachable, since F12's identical guard always fires first; re-derived
  against post-migration code, not just the pre-migration proof it was
  originally written against).

Found-and-rejected (assessed individually against the classifier's
`(type-text, methodName, templateShape) -> family` interface, every
branch in monomorphization's Row F and `ir_lowerer`'s entire Row
G/RT/CH cascade plus both receiver-target/collection-helper files):
**28 branches rejected**, all falling into one of four wrong-shape
categories (the taxonomy the second `ir_lowerer` Step 1b round
established, re-confirmed rather than re-derived this round):

1. **Receiver-type inference** (the converse question, "what type does
   this receiver have" rather than "what family does an already-known
   type/method pair belong to") - Row F's F3, and the entirety of
   `ir_lowerer`'s `resolveMethodReceiverTarget`/RT2/RT3 and G9's
   definition-return-type cascade. This category is exactly what Track 2
   below exists to solve, under a deliberately different interface.
2. **Resolved-path-string classification** (classifies an
   already-resolved semantic-product or definition path string, not a
   normalized binding type text) - G2, G3/G3a-e, G8's
   `isVectorReceiverTarget`/`isKeyValueReceiverTarget` and
   `isBuiltinCollectionTypeName`/`isExperimentalCollectionTypeName`, and
   the `isExplicit*AliasPath`/`isAllowedResolved*DirectCallPath` family.
3. **Method-name-only gates with no type-text input at all** - Row F's
   F1 (literal receiver spelling, not a resolved type), F10, F15/F16;
   `ir_lowerer`'s G1, G3c-iii, G6, and the `preferred*ErrorHelperTarget`
   family.
4. **Fused classification+resolution, or pure error-message selection,
   not a standalone family verdict** - Row F's F2/F6/F8 (F8 delegates to
   a differently-scoped classifier, `CollectionSpellingClassifier`, for
   compat-spelling rather than family); `ir_lowerer`'s G1, G4, G8, G10.

Found-and-fixed as a side effect of Track 1's work: **F14's 5 dead
branches deleted outright** (`TemplateMonomorphMethodTargets.cpp`,
2026-09-09), proven unreachable rather than merely unused, with
byte-identical failing-test-name sets across 3 verification runs proving
the deletion was a pure no-op on behavior.

### Track 2 final tally: `resolveReceiverType`/`CanonicalReceiverType` (receiver-type inference)

Every site Step 1c's 2026-09-10 scoping round named as needing this new
module is now migrated for real (verified this round by `grep`-confirming
`resolveMethodReceiverTypeFromLocalInfo`, the old production path, no
longer exists anywhere - only in comments citing it by name for
historical context):

- **`ir_lowerer`**: `resolveMethodReceiverTarget` (RT2, RT3b's Call-kind
  sub-cascade, RT3c's final fallback) - all three branches migrated,
  `resolveMethodReceiverTarget` is now a thin dispatcher end-to-end over
  `resolveReceiverType`/`resolveReceiverTypeFromCallExpr`/
  `resolveReceiverTypeFromFallbackExpr`.
- **monomorphization**: F3's entire cascade - Name-kind (F3-N1/N2),
  primitive-literal-kind (F3-L/B/Fl/S), and Call-kind (F3-C1/C2/C3b/c/d) -
  all migrated onto `resolveReceiverType`/
  `resolveReceiverTypeFromCallExprForTemplateMonomorph`.
- **F3-C3a** (the receiver-is-a-struct-constructor-call short circuit)
  stays permanently outside this module's scope by design - it is a
  resolution question ("what definition path"), not an inference one
  ("what type"). Preserved exactly via a new
  `structConstructorReceiverPathOut` out-parameter rather than a
  duplicate re-resolution, specifically to avoid double-invoking two
  non-idempotent `...ForTesting` counters a nested method-call receiver's
  resolution can trigger - verified by name against the two counter-
  asserting tests, not merely by matching aggregate pass/fail counts.

This round's own fresh search (grepping for
`inferReceiverType`/`resolveType`-shaped names and structurally similar
patterns across `src/semantics/`, `src/ir_lowerer/`, and the rest of the
tree, beyond the specific functions this effort already touched) found
**no further receiver-type-inference site** duplicating what this module
now centralizes. Track 2's implementation work is complete.

### Found-and-fixed this round: two orphaned diff-audit helper functions deleted

This round's own fresh review surfaced one piece of genuinely dead code
the effort's prior rounds left behind: `isReceiverTargetDiffAuditEnabled()`
and `describeReceiverElementFamily()`
(`include/primec/support/ReceiverElementFamilyClassifier.h`,
`src/support/ReceiverElementFamilyClassifier.cpp`) were declared and
defined but had **zero callers anywhere** - every call site that ever
used them was, by design, migrated to call
`classifyReceiverElementFamilyJoint` for real, and each migration's own
"Step 2" section already retired that call site's own harness wiring;
the two shared helper functions themselves were simply never removed.
Confirmed via exhaustive `grep` (including `tests/`) before deleting.
Removed both, plus the now-unused `<cstdlib>` include their only caller
(`std::getenv`) needed. Same class of finding as the F14 dead-code
deletion earlier in this effort - proven dead, not merely unused in the
common case - and verified the same way: full 3-suite battery run clean
after the change, with the exact same failing-test-case counts and names
as every prior round's recorded baseline (see Verification below).

### Net lines of code

Fresh `git diff --stat` from `039649aaa` (the commit immediately
preceding this effort's first `TODO-5294 Step 0` commit,
`1b4710d04`) to this round's HEAD, restricted to production code:

| scope | insertions | deletions | net |
|---|---|---|---|
| `src/` (6 files) | 1259 | 495 | +764 |
| `include/` (5 files) | 458 | 3 | +455 |
| **production total** | **1717** | **498** | **+1219** |
| `tests/` (2 files) | 219 | 14 | +205 |

The whole effort is net code-*positive*, not negative - this is expected
and consistent with the plan's own stated goal (a new, independently
unit-tested shared module), not a discrepancy to explain away. The two
new shared modules
(`include/primec/support/CanonicalReceiverType.h`,
`CanonicalReceiverTypeSketch.h`,
`include/primec/support/ReceiverElementFamilyClassifier.h`/`.cpp`)
account for roughly 560 lines of new, from-scratch, unit-tested
infrastructure that did not exist before this effort, plus the bulk of
the `include/` insertions above; the remaining growth is substantially
expanded doc-comments at each migrated call site (documenting the exact
behavioral equivalence proof for future readers, per this document's own
"characterize, don't guess" discipline throughout).

Within the migrated call sites specifically - the actual duplicate-logic
removal this effort targeted - each individual migration round's own
`git diff --stat` (recorded at the time, not reconstructed after the
fact) reported a net *deletion* at that call site:

| migration | net lines |
|---|---|
| `resolveArgsPackElementMethodTarget` (Track 1) | ~-90 |
| `resolveMethodTarget` indexed-args-pack cascade (Track 1) | -35 |
| F13/F13b/F13c collection-family slice (Track 1) | -49 |
| F7 File-family slice (Track 1) | -39 |
| F12 generic-Soa slice (Track 1) | -21 |
| RT2 (Track 2, 5 files) | -150 |
| RT3b Call-kind cascade (Track 2) | -287 |
| F3 Name/literal receivers (Track 2) | -40 |
| F3 Call-kind receivers (Track 2) | -166 |
| **sum of the above** | **~-877** |

(F9's and F11's own migration rounds, and RT3c's, reported their
scaffolding as "removed entirely" without a quantified net-line count at
the time, so this sum is a representative partial total drawn from the
rounds that did record one, not an exhaustive accounting of every
migration's own diff.) Read together, the two tables tell a consistent
story: roughly 877 lines of duplicated inline classification/inference
logic (plus the temporary diff-audit harnesses used to prove each
migration safe) were deleted at the specific call sites this effort
touched, while the project as a whole grew by about 1219 net lines
because it also built - once - the shared, tested infrastructure that
duplication is now routed through instead of being re-derived a third or
fourth time at the next call site.

### Final verification: full 3-suite battery against cumulative HEAD

Ran the complete `PrimeStruct_semantics_tests`/
`PrimeStruct_backend_ir_tests`/`PrimeStruct_compile_run_tests` battery,
foreground, against this round's final state (both tracks' full history
plus this round's own dead-code cleanup):

| suite | test cases | failed | doc's recorded baseline |
|---|---|---|---|
| semantics | 2767 | 1 | 2767/1 |
| backend_ir | 1646 | 46 | 1646/46 |
| compile_run | 2679 | 5 | 2679/5 |

All three counts match this document's own last-recorded baseline
exactly - no drift across the cumulative effort. Semantics' single
failure is the same already-known flake named throughout this document
(`semantic product validates direct return method-like borrowed
helper-return experimental soa reads`), confirmed by name in this
round's own run, not merely by count.

### Status: TODO-5294 closed

Both consolidation tracks - `classifyReceiverElementFamilyJoint`
(receiver-family classification) and `resolveReceiverType`/
`CanonicalReceiverType` (receiver-type inference) - are migrated
everywhere a genuine fit was found, with every remaining branch in both
tracks' scope individually assessed and rejected for a specific,
documented, non-`(type, methodName)`-shaped reason rather than left
unexamined. This round's fresh, independent search for a missed
receiver-type-inference site found none. TODO-5294 is marked closed in
`docs/todo.md` (moved to `docs/todo_finished.md`) on this basis.
