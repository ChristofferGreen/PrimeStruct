# Expression Rewrite Refactor Plan

Status: planned, not started. Queued behind the current all-tests-green
verification campaign on the overload/classifier branch. Executes with
the same per-slice gating discipline proven by
`docs/CompatPathResolutionConsolidation.md` (clean-tree builds, suite
gates per commit, corpus name-level diffs, golden pins updated in
lockstep).

## The Problem

`rewriteExpr` in `src/semantics/TemplateMonomorphExpressionRewrite.h` is
one function spanning essentially the whole 3,440-line header (it begins
at line 35), with 28 separate `expr.name = ...` assignment sites. It is
the monomorphization expression-rewrite pass: it turns surface call
expressions into resolved, specialized internal targets before semantic
validation and IR lowering consume them.

Why it resists change — each point observed directly while working on it,
not speculation:

1. **Order-dependent in-place mutation.** `expr.name`,
   `expr.namespacePrefix`, and `expr.templateArgs` are rewritten
   sequentially; every one of the ~28 write sites both depends on and
   changes what all later blocks see, alongside a `resolvedPath` local
   that is recomputed and reassigned at several points. Inserting logic
   anywhere requires reasoning about everything below the insertion
   point. (This is how the first Step 2a attempt on the consolidation
   branch broke the stdlib SOA template machinery: a canonicalization
   inserted upstream starved the same-path helper spellings that
   later-running machinery consumes mid-rewrite.)
2. **Unnamed phases.** There is a real implicit pipeline — nested
   experimental-constructor rewrites, then the path-preference cascade
   (collection compat/SOA/borrowed-wrapper preferences plus the
   spelling-classifier consultation), then template-argument inference,
   then instantiation, then constructor-argument rewrites — but nothing
   names or enforces it. Ordering is expressed only as vertical position
   inside one function.
3. **Duplicated twin cascades.** The direct-call block (~lines
   2080-3040) and the method-call block (~3045-3400) contain
   near-verbatim copies of the same cascades (SOA helper preference,
   synthetic template-carry clearing, canonical collection-helper
   preference, template inference + instantiation), which have already
   drifted in small ways (e.g. the method side unconditionally rewrites
   `expr.name` where the direct side gates on overload-family
   membership).
4. **Golden source-shape pins.** Several ir_pipeline "source delegation
   stays stable" tests pin exact code text from this header and its
   callees. Restructuring pays lockstep pin updates — a known, budgetable
   cost (paid four times during the consolidation work), not a blocker.
5. **Verification cost.** Full-corpus gates take hours in the current
   environment, so slices must be small, independently shippable, and
   individually gated.

## Why Now Is Cheaper Than Before

Assets that did not exist when this function last grew:

- `CollectionSpellingClassifier` already owns the compat-spelling
  decisions several of the cascade blocks used to compose by hand, and
  `isResolutionStageCollectionSpellingPrefix` marks the stage-scoping
  rule explicitly.
- `Expr::resolvedCallPath` capture marks the function's two settling
  points (direct-call and method-call), which are natural phase
  boundaries.
- The publication-vs-resolution staging rule (consolidation doc, Step 2a
  findings) documents the one known trap: bare `/soa/*` same-path
  spellings are consumed mid-rewrite and must not be canonicalized here.
- The gating method (clean-tree builds only, per-commit suite gates,
  corpus name-diffs, differential audit hooks where two implementations
  must agree) is established and has caught every regression this branch
  produced before it shipped.

## Plan

### Step 0 — Characterize: the phase table

Before moving any code, build a table of `rewriteExpr`'s blocks in
execution order. Per block: trigger condition (what shapes/spellings
enter it), state read (which prior mutations it depends on), state
written (`expr` fields, `resolvedPath`, `allConcrete`, context caches),
early exits (returns true/false vs falls through), and which tests pin
it (behavioral and source-shape). Deliverable: the table appended to
this document, with any block whose dependencies cannot be stated
confidently flagged for a targeted experiment rather than guessed.

Known rows to capture from session experience: the `ct_if` recursion
special case; bare-name/local-binding resolution; tuple and pack-index
rewrites; the direct-call cascade including the map-constructor
named-pair logic; the SOA same-path/borrowed/public-mutator preference
chain and its mid-rewrite spelling consumers; template inference +
`instantiateTemplate`; the method-call twin cascade including the
static-FileError special case; the final constructor-argument rewrite
calls.

### Step 1 — Mechanical extraction

Carve blocks into named phase functions over an explicit state struct
(shape to be settled in Step 0, roughly
`RewriteState { Expr &expr; std::string resolvedPath; bool allConcrete;
... }`), one block (or a few trivially-coupled blocks) per commit. Zero
logic change per commit; each gated on the semantics/ir_pipeline suites
plus the pinned golden tests, with pin updates in the same commit. The
extraction order should start at the leaves (blocks with no downstream
dependents, e.g. the trailing constructor-argument rewrites) and move
upward, so each commit's blast radius shrinks rather than grows.

### Step 2 — De-duplicate the twin cascades

Where the direct-call and method-call cascades are provably identical
after extraction, merge them into shared helpers. Where they differ, the
difference becomes a named, commented, decided divergence (or a bug fix
with its own pinning test) instead of drift. Candidate first merges: the
synthetic same-path SOA template-carry clearing (verbatim twice today)
and the collection-helper preference calls.

### Step 3 — Explicit pipeline

The extracted phases become an ordered, named sequence (a plain ordered
list of function calls with the state struct — no framework), so future
rules get a declared insertion point and the phase order is reviewable
in one screen. If Step 0 reveals genuinely dynamic ordering (phases that
re-enter or loop), that constraint gets documented in the pipeline
rather than flattened away.

## Non-Goals

- No behavior changes to what any program resolves to or which
  diagnostics fire — this is structure only. Any bug found en route is
  fixed in its own separately-gated commit with a pinning test.
- No new abstraction framework (no visitor/rule-engine machinery); named
  functions and one state struct.
- No relocation of the publication-stage rows (bare `/soa/*`
  canonicalization stays validator-owned per the consolidation doc).
- Not attempting to merge this pass with the validator's rewrite
  machinery — one function at a time.

## Risks

- **Mid-rewrite spelling consumers** (the Step 2a trap): the phase table
  must mark which downstream machinery consumes intermediate spellings
  before any block that produces them is moved.
- **Golden pin churn**: each extraction commit updates the source-shape
  pins it disturbs; the pins remain guards, not casualties.
- **Environment**: full-corpus gates are hours; stale-object artifacts
  have produced phantom failures before. All gating on fresh/clean build
  trees, and any surprising failure gets the clean-rebuild triage before
  code-level debugging.
