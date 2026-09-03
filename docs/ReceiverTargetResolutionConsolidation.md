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
