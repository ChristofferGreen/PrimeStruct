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
