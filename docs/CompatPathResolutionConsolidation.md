# Compat-Path Resolution Consolidation Plan

Status: refactor plan, not yet started. This is the prerequisite discovered
by (and blocking the remainder of) Phase 0 in
`docs/OverloadResolutionPrototype.md`. When the consolidation lands, Phase 0
resumes on top of it; when Phase 0 lands, the type-based overload work in
that document resumes on top of Phase 0.

## The Problem, Verified

Resolving a collection-helper call spelling (e.g. `/soa/get`,
`/array/push`, a legacy key-value alias) is governed by at least three
interacting rules, and none of them has a single implementation:

1. **Compat-to-canonical renaming** — a legacy/compat spelling maps to a
   canonical stdlib surface path. Implemented independently in:
   - `SemanticsValidator::preferredCollectionHelperResolvedPath`
     (`SemanticsValidatorBuildInitializerInference.cpp:104-216`), gated on
     `defMap_` presence for key-value/vector but *not* for the SOA branch
   - monomorphization's `resolveStdlibSurfaceCompatibilityAlias` /
     `resolveRootedStdlibSurfaceCompatibilityPath`
     (`TemplateMonomorphTypeResolution.h:694-763`), gated on scoped-import
     visibility
   - `preferVectorStdlibHelperPath` / `preferVectorStdlibTemplatePath`
     (`TemplateMonomorphCollectionCompatibilityPaths.h:95-162`), gated on
     `defs.count(...) == 0`, targeting a partially-defunct legacy family
2. **Removed-helper rejection** — certain retired spellings must fail with
   "unknown method"-shaped diagnostics rather than resolve. The predicates
   (`isRemovedVectorCompatibilityHelper`,
   `isRemovedBorrowedSoaCompatibilityHelper`,
   `isRemovedKeyValueCompatibilityHelper`,
   `TemplateMonomorphCollectionCompatibilityPaths.h:4-23`) are consulted in
   **9 files spanning all three stages**: monomorphization
   (`TemplateMonomorphTypeResolution.h`,
   `TemplateMonomorphExpressionRewrite.h`), the validator
   (`SemanticsValidatorStatementBodyArguments.cpp`,
   `SemanticsValidatorExprMethodTargetResolution.cpp`,
   `SemanticsValidatorBuildCallResolution.cpp`,
   `SemanticsBuiltinPathHelpers.cpp`), and IR lowering
   (`IrLowererSetupTypeCollectionHelpers.cpp/.h`). The corresponding
   "unknown method:" diagnostics are emitted from **15 files**.
3. **Same-path shadow precedence** — a user-defined definition at the
   exact compat path (e.g. their own `/soa/ref`) must win over both
   renaming and rejection. Enforced only implicitly, wherever a given
   implementation happens to check its own stage's definition map first
   (`ctx.sourceDefs` in monomorphization, `defMap_` in the validator,
   `defMap` in `ir_lowerer`).

Nothing enforces agreement between these implementations. The low-level
*primitives* (`splitSoaSurfaceHelperPath`, `publicSoaHelperTargetPath`,
`isExplicitPublicSoaSurfaceHelperName`, the `StdlibSurfaceRegistry`
metadata) are shared; the *composed decisions* built from them are not.

## Evidence This Is Load-Bearing, Not Theoretical

The Phase 0 work in `docs/OverloadResolutionPrototype.md` hit this
directly. A differential audit (instrumenting
`preferredCollectionHelperResolvedPath` to compare its answer against
monomorphization's captured `Expr::resolvedCallPath`, then running the
semantics and ir_pipeline corpora) proved the resolvers disagree on real
programs (`/soa/get`, `/soa/ref`). An attempt to close the gap by porting
the SOA canonicalization into monomorphization then failed twice, each
time on a *different* rule that the ported code didn't know about:

- first on shadow precedence (a user-defined `/soa/ref` stopped winning)
- then, after gating on definition existence, on removed-helper rejection
  (tests expecting `/soa/push`/`/soa/reserve` to be rejected as "unknown
  method" in method-call contexts started resolving successfully)

Serial discovery of interacting rules is the signature of logic that needs
to be characterized and centralized, not patched in place. That attempt
was reverted in full.

## Goal

One authoritative decision function — a *spelling classifier* — that every
stage calls instead of composing the primitives itself:

```cpp
// Conceptual shape, not final signature.
enum class CompatSpellingDisposition { PassThrough, Canonicalize, Reject };

struct CompatSpellingDecision {
  CompatSpellingDisposition disposition;
  std::string canonicalPath;   // when Canonicalize
  std::string_view rejectReason; // when Reject, for diagnostic assembly
};

CompatSpellingDecision classifyCollectionHelperSpelling(
    std::string_view path,
    CallShape shape,              // direct call vs method call vs binding init
    const ScopeFacts &scope,      // import visibility for this call site
    const DefinitionExistsFn &definitionExists); // stage-supplied lookup
```

Design constraints learned from the failed attempt:

- **Shadow precedence is part of the rule, not the caller's problem.** The
  classifier itself checks `definitionExists(path)` first and returns
  `PassThrough`, so no caller can forget it. The lookup is a callback
  because each stage legitimately has a different definition map
  (`ctx.sourceDefs`/`ctx.helperOverloads` vs `defMap_` vs `ir_lowerer`'s
  `defMap`); the *rule* is shared even though the *map* is not.
- **Rejection is checked before canonicalization**, so a removed spelling
  can never be rescued by the rename path — the exact bug the reverted
  attempt shipped.
- **Call shape is an explicit input.** The corpus shows rejection is
  context-dependent (method-call contexts reject spellings that other
  contexts may treat differently); today that context is implicit in
  *which* of the 9 files happens to run. The classifier makes it a
  parameter with enumerable values.
- **Data lives in `StdlibSurfaceRegistry`.** The classifier composes
  registry metadata; it does not grow its own hardcoded spelling tables.
  Migrating the hardcoded lists in
  `TemplateMonomorphCollectionCompatibilityPaths.h:4-23` into registry
  metadata is in scope.

## Plan

### Step 0 — Characterize (no production-code changes)

Write the rule table before writing the function. For each compat spelling
family (vector, borrowed-SOA, key-value, public-SOA) × call shape (direct,
method, binding initializer) × shadow-definition-present (yes/no) ×
import-visibility state, record the expected disposition and which test
pins it. Sources:

- the three `isRemoved*` predicate lists and every consumer's surrounding
  condition (the 9 files above)
- `preferredCollectionHelperResolvedPath`'s three branches and their
  different `defMap_` gating
- the "unknown method:" emission sites (15 files) — which spelling +
  shape combinations each one actually fires for
- the tests that regressed during the reverted attempt
  (`test_semantics_calls_and_flow_collections_container_error_and_result_helpers.cpp`
  cases around lines 1691, 1807, 3785, 4059) as known-tricky rows

Deliverable: a rule table appended to this document. Rows the corpus does
not pin get explicit "unpinned — current behavior is X by accident of
implementation" annotations; those need a decision (and a new test) before
Step 2 may change them.

### Step 1 — Build the classifier plus a permanent differential harness

Implement `classifyCollectionHelperSpelling` against the Step 0 table,
with direct unit tests per table row. Simultaneously, make the
differential methodology from Phase 0 permanent instead of throwaway:
a build-flag- or env-gated check (e.g.
`PRIMESTRUCT_RESOLUTION_DIFF_AUDIT=1`) that, at each legacy call site,
computes both the legacy answer and the classifier's answer and reports
divergence, so the whole existing test corpus doubles as a differential
corpus on demand. The one-test-flip observed during Phase 0
("soa pending diagnostics route through shared semantics helpers") showed
even the *existing* implementations disagree today; the harness makes
every such latent disagreement enumerable instead of anecdotal.

### Step 2 — Migrate stage by stage, behavior-preserving

Order: monomorphization first (it feeds `Expr::resolvedCallPath`, which
Phase 0 already carries downstream), then the validator's composed sites
(`preferredCollectionHelperResolvedPath` becomes a thin wrapper over the
classifier, then its 33 call sites stop needing to exist separately), then
`ir_lowerer`. Each migration ships only when the differential harness
reports zero divergence across the semantics, ir_pipeline, and compile_run
corpora, with any intended divergence (a bug fix agreed from the Step 0
table) called out per-commit.

Known cost: the golden source-text tests that pin `ir_lowerer` file
internals (e.g.
`test_ir_pipeline_validation_ir_lowerer_collection_helper_rewrite_guards.cpp`
pinning `IrLowererLowerStatementsExpr.h`) must be updated in lockstep —
they assert the *shape* of exactly the code this step replaces.

### Step 3 — Resume Phase 0 on top

With one classifier feeding `resolvedCallPath`, the blocked remainder of
`docs/OverloadResolutionPrototype.md` Phase 0 (remaining validator sites,
the publication step, the 10 `ir_lowerer` files' `__ov` scans) becomes the
read-instead-of-rederive substitution it was originally scoped as.

## Step 0 Findings

Step 0 characterization is complete. Two parallel audits (one over the
rejection rules, one over canonicalization and shadow precedence) produced
the rule table below and several findings that amend the plan itself.

### Correction: rejection is three mechanisms, not one

The plan's original framing named only the `isRemoved*` predicate family.
The audit found three independent mechanisms that all emit
"unknown method:"-shaped rejections:

- **Mechanism A — the `isRemoved*` predicates**
  (`TemplateMonomorphCollectionCompatibilityPaths.h:4-23` and clones).
  Covers explicit rooted/prefixed spellings (`/array/x`, rooted
  `/vector/x`, `map/x`, borrowed-SOA `*_ref` helpers).
- **Mechanism B — the SOA pending/unavailable surface**
  (`soaUnavailableMethodDiagnostic` → `canonicalSoaPendingHelperPath`,
  `SemanticsBuiltinPathHelpers.cpp:1114-1150`, driven from
  `SemanticsValidatorExprMapSoaBuiltins.cpp:485-593` and
  `SemanticsValidatorExprMethodResolution.cpp:744-768`). This — not A —
  rejects `.push`/`.reserve`/`.ref`/`.get_ref` method calls on SOA
  receivers. `ref`, `push`, `reserve` appear in **no** `isRemoved*` set
  applicable to SOA receivers. Its shadow gate is
  `hasVisibleDefinitionPathForCurrentImports("/soa/<h>")`
  (`ExprMapSoaBuiltins.cpp:525-530, 560-583`).
- **Mechanism C — the removed-collection-method-path builders**
  (`removedCollectionMethodPath`, `resolveRemovedCollectionHelperReference`,
  `isPublishedVectorMutatorHelperName`, `isPublishedKeyValueBaseHelperName`,
  `SemanticsValidatorInferCollectionCompatibility.cpp:816-926`), feeding
  reject sites at `SemanticsValidatorInferPreDispatchCalls.cpp:382` and
  `SemanticsValidatorExprMethodResolution.cpp:216`. Also independent of A.

All four tests that broke the reverted attempt are pinned by **Mechanism
B**. The classifier must absorb A, B, and C; migrating only the `isRemoved*`
lists into registry metadata would re-ship the reverted bug.

Rejection is also **explicit or emergent depending on spelling shape**:
explicitly-namespaced spellings hit an explicit check; bare spellings whose
canonicalization is *declined* upstream are rejected emergently by generic
definition-existence failure. The reverted attempt broke precisely by
removing a decline step, letting bare removed spellings resolve instead of
falling through to emergent rejection. The classifier's `Reject` vs
`PassThrough` distinction must preserve this: pass-through is load-bearing.

### Predicate duplication with divergent implementations

`isRemovedVectorCompatibilityHelper` / `isRemovedKeyValueCompatibilityHelper`
are defined six times: hardcoded name lists (10 / 13 names) in
`TemplateMonomorphCollectionCompatibilityPaths.h:4-23`,
`SemanticsValidatorStatementBodyArguments.cpp:14-29`,
`SemanticsBuiltinPathHelpers.cpp:17-36`,
`SemanticsValidatorBuildCallResolution.cpp:391-405`; registry-membership
variants in `SemanticsValidatorExprMethodTargetResolution.cpp:17-24`
(`CollectionsManifestSurface0/2` + a bolted-on `"size"`) and
`ir_lowerer/IrLowererSetupTypeCollectionHelpers.cpp:393-397`. If registry
membership ever drifts from the hardcoded lists, stages disagree silently.
No test asserts the implementations agree. UNPINNED.

### Registry gaps and dead code

- The bare `/soa/...` and `/soa_vector/...` spellings are **absent from
  `StdlibSurfaceRegistry`** — only the validator knows them, via hardcoded
  predicates (`SemanticsBuiltinPathHelpers.cpp:265-320`). This is the root
  cause of the monomorphization/validator SOA divergence, and means Step
  1's "data lives in the registry" requires adding these spellings as
  compat metadata, not just reading what's there.
- The SOA halves of `preferVectorStdlibHelperPath` /
  `preferVectorStdlibTemplatePath`
  (`TemplateMonomorphCollectionCompatibilityPaths.h:102-115, 134-149`)
  target `/std/collections/soa_vector/...`, a family with **zero
  definitions** (no `soa_vector.prime` exists). Those branches can never
  fire — dead code, delete during Step 2a. Only the `/array/` branch is
  live.
- The validator's SOA compat sub-branch can emit
  `/std/collections/soa_vector/<h>` — a path with **no backing
  definition** — via `preferredSoaHelperTargetForCollectionType`'s
  fallback (`SemanticsValidatorBuildInitializerInference.cpp:211-213`
  → `SemanticsValidatorPrivateCore.h`-declared member, fallback at its
  line ~846). Treated as a bug (decision D5 below).
- Latent bug: `explicitStdSoaHelperName`'s first clause calls
  `usesExplicitPublicSoaHelperPath(normalizedPrefix, {})` with an empty
  second argument (`SemanticsValidatorBuildInitializerInference.cpp:158-161`),
  collapsing it to a pure prefix test; masked because a later branch
  re-derives the answer and the correct two-arg call is redone at
  `:204-205`.
- Diagnostic-wording coupling: monomorphization emits
  `"unknown call target:"` where the validator emits `"unknown method:"`
  for the same rejection class, and
  `SemanticsValidatorStatementReturns.cpp:112-115` **string-parses** the
  `"unknown method: /<alias>/<helper>"` wording to classify an
  already-produced diagnostic. Any message normalization must update that
  matcher in lockstep.

### Rule table

Legend: disposition is current behavior. `DECLINE` = decline to
canonicalize/resolve at that site, leaving the spelling for downstream
(usually emergent rejection). Removed-name sets: vector-removed =
`{count, capacity, at, at_unsafe, push, pop, reserve, clear, remove_at,
remove_swap}`; borrowed-soa-removed = `{count_ref, get_ref, ref_ref,
to_aos_ref}`; kv-removed = `{count, count_ref, size, contains,
contains_ref, tryAt, tryAt_ref, at, at_ref, at_unsafe, at_unsafe_ref,
insert, insert_ref}`; soa-supported-readers = `{get, get_ref, ref,
ref_ref, count, count_ref, soa, single, from_aos, field_view}`.

| # | family | spelling | call shape | shadow? | imports | current disposition | enforced at | pinned by |
|---|---|---|---|---|---|---|---|---|
| 1 | public-SOA | bare `/soa/<supported-reader>` | direct / binding init | no | any | validator: CANONICALIZE → `/std/collections/soa/<h>` (unconditional, no def check); monomorph: PASS THROUGH (divergence D1) | `SemanticsValidatorBuildInitializerInference.cpp:202-213` | UNPINNED behaviorally (source-text golden only: `test_ir_pipeline_validation_emitter_expr_control_if_branch_emit_step_composes_value_and_handlers.cpp:642+`) |
| 2 | public-SOA | `.ref(...)` | method call on `SoaVector` receiver | no | `/std/collections/soa/*` | REJECT `unknown method: /soa/ref` (Mechanism B) | `SemanticsValidatorExprMapSoaBuiltins.cpp:515-585` | `container_error_and_result_helpers.cpp:1691` |
| 3 | public-SOA | `.push(...)`, `.reserve(...)` | method call on `SoaVector` | no | soa/* | REJECT `unknown method: /soa/reserve` etc. (Mechanism B) | `SemanticsValidatorExprMethodResolution.cpp:756-768`, `SemanticsBuiltinPathHelpers.cpp:1148-1150` | `:1807` |
| 4 | public-SOA | `.get(...)` | method call on `SoaVector`, incl. struct-helper-return receiver | no | soa/* | RESOLVE (supported access) | `SemanticsValidatorExprCollectionAccess.cpp:710-737` | `:3785` |
| 5 | public-SOA | `.ref(...)` | method call, struct-helper-return receiver | YES: user `/soa/ref` def | soa/* | RESOLVE to shadow (shadow beats retirement) | `ExprMapSoaBuiltins.cpp:525-530, 560-583` | `:4059` (contrast `:4093`) |
| 6 | soa-compat | `/soa/<h>`, ambiguous receiver, no visible same-path def | direct / binding init | no | any | validator emits `/std/collections/soa_vector/<h>` — no backing def exists (bug, D5) | `BuildInitializerInference.cpp:211-213` + member fallback | UNPINNED |
| 7 | soa_vector | `/std/collections/soa_vector/*` | any | — | — | dead family; monomorph prefer* SOA branches can never fire | `TemplateMonomorphCollectionCompatibilityPaths.h:102-115, 134-149` | dead code |
| 8 | borrowed-SOA | `<borrowed-soa-removed>` receiver-method alias | method call | n/a | any | monomorph DECLINE-to-resolve → emergent unknown downstream | `TemplateMonomorphMethodTargets.h:458` via `isExplicitRemovedCollectionMethodAlias` | UNPINNED directly |
| 9 | vector | `/array/<vector-removed>` | direct call | def at `/array/<h>` wins | any | monomorph DECLINE canonicalization (leaves `/array/x`) | `CompatibilityPaths.h:116-125, 150-159` | UNPINNED |
| 10 | vector | `/array/<vector-removed>` under explicit array namespace | method call | no | any | REJECT `unknown method: /array/<h>` | `SemanticsValidatorExpr.cpp:642` | vector corpus |
| 11 | vector | `/array/<non-removed>` | direct / method | `defs.count(path)==0` gate (shadow wins) | any | CANONICALIZE → `/std/collections/vector/<h>` when canonical def exists | `CompatibilityPaths.h:116-125` (monomorph); `SemanticsValidatorCollectionHelperRewrites.cpp:316` (validator member) | ir_pipeline golden |
| 12 | vector | rooted `/vector/<vector-removed>` | method call | receiver-compatible explicit shadow wins | any | REJECT `unknown method` if no shadow; RESOLVE if shadow | `SemanticsValidatorExprMethodTargetResolution.cpp:2821-2836` | vector corpus |
| 13 | vector | `<canonical-vector-ns>/<vector-removed>` | direct call w/ prefix | literal shadow may match | any | PASS THROUGH to literal removed path (emergent rejection if no def) | `SemanticsValidatorBuildCallResolution.cpp:449-452` | — |
| 14 | vector | `std/collections/vector/<h>` canonical-namespaced | direct / binding init | gated on `defMap_` canonical presence | any | CANONICALIZE if canonical def present, else PASS THROUGH | `BuildInitializerInference.cpp:192-200` | UNPINNED behaviorally |
| 15 | key-value | `map/<kv-removed>` | method call with args, map-root prefix | no | any | REJECT `unknown method: /map/<h>` | `SemanticsBuiltinPathHelpers.cpp:665-692` + 4 consumers | KV corpus |
| 16 | key-value | removed KV compat path, helper ∈ {count, count_ref, size} | direct call (collection-helper rewrite) | sourceDefs/templateDefs/helperOverloads presence rescues | any | REJECT `unknown call target:` if absent; CANONICALIZE if in templateDefs | `TemplateMonomorphExpressionRewrite.h:2434-2475` | KV corpus (indirect) |
| 17 | key-value | rooted KV alias e.g. `/map/count` | direct call | pass-through preserves shadow reachability | non-stdlib scope | monomorph DECLINE stdlib-surface rename (decline-before-canonicalize invariant) | `TemplateMonomorphTypeResolution.h:767-773, 791, 804` | — |
| 18 | key-value | KV import-alias spelling, canonical def present | direct / binding init | gated on `defMap_` | any | validator CANONICALIZE → `/std/collections/map/<member>`; monomorph PASS THROUGH in user code (D6) | `BuildInitializerInference.cpp:182-190` vs `TypeResolution.h:695, 733` | UNPINNED behaviorally |
| 19 | key-value | KV body-argument target, removed helper | binding init | — | any | validator CANONICALIZE (removed → canonical short-circuit) | `SemanticsValidatorStatementBodyArguments.cpp:134-136, 285, 353` | — |
| 20 | any registry surface | compat/lowering spelling inside stdlib-owned code | direct call | compat-path user def wins (bail) | stdlib-scoped only | CANONICALIZE → `metadata.canonicalPath/member` if canonical def exists, else PASS THROUGH | `TemplateMonomorphTypeResolution.h:694-763` | stdlib corpus |
| 21 | vector / key-value | `/array/<removed>`, `/std/collections/vector/<removed>`, KV alias | method target (IR lowering) | registry-metadata gate | any | REJECT `unknown method: ` + resolved | `IrLowererSetupTypeMethodTargetHelpers.cpp:228-288`, `IrLowererSetupTypeCollectionHelpers.cpp:393-425, 661-685` | ir_pipeline golden |
| 22 | vector / key-value | published mutator / base helper (Mechanism C) | method / direct | `hasExplicitDefinitionFamilyPath` / `defMap_` gates | any | REJECT `unknown method` | `InferCollectionCompatibility.cpp:816-926` → `InferPreDispatchCalls.cpp:382`, `ExprMethodResolution.cpp:216` | — |

### Cross-stage divergences requiring a decision

Governing principle adopted for all of them: **the validator's current
answer is the reference behavior.** It is what users observe (diagnostics)
and what the publication step publishes to `ir_lowerer`; monomorphization's
`resolvedCallPath` is new and consumed at only five low-risk sites, so it
is the side to bring into line. Where the validator is *internally*
inconsistent, pinned tests define the answer; unpinned conflicts get the
simplest consistent rule, called out per-commit.

- **D1** (row 1): bare `/soa/<supported-reader>` direct calls, no shadow —
  canonicalize (validator behavior). Monomorph adopts via classifier.
  Method-call retirement (rows 2-3) must be absorbed first, which is what
  the reverted attempt missed.
- **D2**: predicate sextuplication — single registry-backed source, with
  the hardcoded lists migrated into registry metadata and a test asserting
  the effective sets match today's 10/13/4 name lists exactly.
- **D3** (rows 9, 10, 13): `/array/<removed>` has four different
  treatments by entry point. Keep pinned outcomes; unify unpinned ones to
  "decline/pass-through in direct-call shapes (shadow reachability),
  explicit reject in method shapes on collection receivers."
- **D4**: diagnostic wording (`unknown call target:` vs `unknown
  method:`) stays as-is per shape for now (behavior-preserving); the
  `StatementReturns.cpp:112-115` string matcher is noted as lockstep-update
  coupling for any future normalization.
- **D5** (row 6): the `/std/collections/soa_vector/<h>` no-definition
  fallback is a bug; classifier never emits a path without a backing
  definition — that input becomes PASS THROUGH (emergent rejection). Needs
  a new pinning test.
- **D6** (rows 14, 18): KV/vector canonicalization in user code —
  validator behavior (canonicalize when canonical def visible in
  `defMap_`) becomes the rule; monomorph's stdlib-scope-only gating is
  subsumed. Needs new pinning tests since these rows are currently
  behaviorally unpinned.

### Amendments to the classifier design (feeds Step 1)

- Inputs must include **receiver family** (vector/soa/map/none) in
  addition to call shape — Mechanism B keys on SOA receivers, and row 10
  vs row 9 shows shape+receiver together decide `/array/*` disposition.
- The classifier's output vocabulary needs `Decline`/`PassThrough` to be
  first-class (not folded into "no answer") because pass-through is
  load-bearing for both shadow reachability (rows 13, 17) and emergent
  rejection.
- Mechanisms B and C are in scope for absorption, not just Mechanism A's
  lists. The bare `/soa/*` spellings and the pending/retired SOA member
  sets must be added to `StdlibSurfaceRegistry` metadata.
- Unit tests must pin: the six-way predicate agreement (D2), rows 1-5's
  full matrix, D5's corrected behavior, and D6's newly-decided rows.

## Non-Goals

- No behavior changes to which spellings are accepted, rejected, or
  renamed, except rows the Step 0 table explicitly flags and a test
  pins — this is consolidation, not compat-surface redesign.
- No change to the `StdlibSurfaceRegistry` public surface beyond hosting
  the migrated spelling metadata.
- Method-call *receiver* inference (which type a method call dispatches
  on) stays where it is; the classifier only decides spelling
  disposition, not receiver typing.

## Risks

- **The rule table may expose genuine inconsistencies** between stages
  (the differential audit already found one test's worth). Each one needs
  a decision — which behavior is intended — not a silent pick. Budget for
  a few small behavioral decisions surfacing.
- **Environment noise in verification.** During Phase 0, identical
  full-suite runs varied 2-3x in wall-clock time, and one run stalled
  without conclusive diagnosis. Verification gates should be defined on
  test *outcomes* (name-level diffs), never on timing, and long runs
  should be assumed interruptible.
- **This is the second consolidate-before-extending prerequisite found
  under the same feature.** If Step 0 uncovers a third layer of the same
  shape, that is a signal to stop and reassess how much of the semantic
  stage wants this treatment, rather than chaining prerequisite refactors
  indefinitely.
