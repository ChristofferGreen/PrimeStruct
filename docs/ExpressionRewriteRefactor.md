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
6. **Recursion.** `rewriteExpr` calls itself (lines 58, 191, 210, and
   more) to descend into `bodyArguments`, lambda parameters/bodies, and
   argument subtrees. The phases are per-node stages inside a recursive
   descent, not a flat pass over a list — the extracted state must be
   per-invocation, and any phase that recurses must keep doing so through
   the public entry point.
7. **Textual-include architecture.** This header (like all
   `TemplateMonomorph*.h` siblings) is textually included into
   `TemplateMonomorph.cpp` in a fixed order, with free functions that
   must be defined (or forward-declared in `TemplateMonomorph.cpp`'s
   declaration block) before use. Extracted phase functions must live in
   this header above `rewriteExpr`, or in a new header slotted earlier in
   the include chain, or gain forward declarations — position is a build
   constraint, not a style choice.

Known facts an executor needs (verified):

- Entry signature to wrap in the state struct (9 parameters):
  `rewriteExpr(Expr &expr, const SubstMap &mapping, const
  std::unordered_set<std::string> &allowedParams, const std::string
  &namespacePrefix, Context &ctx, std::string &error, const LocalTypeMap
  &locals, const std::vector<ParameterInfo> &params, bool allowMathBare)`,
  plus the function-local `resolvedPath` and `allConcrete` threads.
- Source-shape pin holders for this header:
  `tests/unit/test_stdlib_map_ownership.cpp` and
  `tests/unit/ir_pipeline/test_ir_pipeline_validation_emitter_expr_source_delegation_stays_stable.cpp`
  (both read the file from disk and assert literal code text; budget a
  lockstep update in every extraction commit that moves pinned text).
- Landmark line anchors (as of the Phase 1 branch head; re-derive before
  executing): function begins at :35; `ct_if` special case ~:48-72;
  bare-name/local resolution ~:110-230; tuple/pack rewrites ~:660-2079;
  direct-call block ~:2080-3046; method-call block ~:3048-3400;
  `expr.resolvedCallPath` settling points at ~:2984 (direct) and
  ~:3366-3372 (method).

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

## Gate Policy

Per-commit (minutes): build the semantics + ir_pipeline test binaries in
a fresh-or-known-clean tree and run the quick set — the overload family
tests (`--test-case="*overload*,same-arity*"`), the classifier suite
(`primestruct.semantics.collection_spelling_classifier`), the five-test
SOA gauntlet, and the two pin-holder golden tests named above.

Per-milestone (hours; end of Step 1, end of Step 2, end of Step 3): the
sharded CTest gate (`ctest --test-dir <build> --output-on-failure
--parallel N`), which is the repository's authoritative green signal. Do
NOT gate on running a whole doctest binary unsharded in one process:
`docs/failing_tests.md` documents deterministic cross-test-case state
pollution in that mode (100+ phantom failures in
`semantics.calls_flow.collections` alone, byte-identical on unmodified
baselines). If an unsharded corpus run is used for a quick comparison
anyway, only name-level failing-set diffs against a same-command baseline
run are meaningful — never the raw failure count.

Any surprising failure gets two triage steps before code-level debugging:
re-run the failing case in isolation (`--test-case=...`, which matches the
sharding the real gate uses), and the clean-rebuild triage (fresh build
tree) — both cross-case pollution and stale-object artifacts have
produced convincing phantom failures in this repo before.

## Execution Checklist

Step 0 — characterize (no production edits):
- [x] Re-derive the landmark anchors against the executing branch head
      (verified at `78407d5`: function `:35`, ct_if `:48-72`, direct-call
      `:2080-3047`, method-call `:3048-3396`, resolvedCallPath settling
      `:2981` / `:3364-3372`)
- [x] Build the phase table: one row per block, in execution order, with
      trigger condition, state read, state written, early-exit behavior,
      recursion sites, downstream mid-rewrite consumers, and pinning
      tests; append it to this document (see "Phase Table" below)
- [x] Flag every row whose dependencies cannot be stated confidently and
      attach a targeted experiment (not a guess) per flagged row (rows
      R9, R19, R21 flagged in the table's "Flagged rows" subsection)
- [x] Exit criterion: every line of `rewriteExpr` is owned by exactly one
      row; the mid-rewrite spelling-consumer rows (bare `/soa/*`
      same-path machinery) are explicitly marked (R16c, R19f)

Step 1 — mechanical extraction (one commit per block or small coupled
group, leaf-first):
- [ ] Define the per-invocation state struct wrapping the 9-parameter
      signature plus `resolvedPath`/`allConcrete`
- [ ] Extract trailing constructor-argument rewrite calls (no downstream
      dependents) as the pilot slice
- [ ] Extract the method-call cascade blocks, bottom-up
- [ ] Extract the direct-call cascade blocks, bottom-up
- [ ] Extract the pre-cascade blocks (tuple/pack, bare-name, `ct_if`)
- [ ] Each commit: zero logic change, pin updates in the same commit,
      per-commit gates green
- [ ] Milestone gate: full corpora name-identical
- [ ] Exit criterion: `rewriteExpr` body is a readable sequence of named
      phase calls (target: under ~150 lines), all behavior identical

Step 2 — de-duplicate the twins:
- [ ] Textually diff the extracted direct-call vs method-call phase
      functions; classify each pair identical / drifted
- [ ] Merge identical pairs (first candidates: synthetic same-path SOA
      template-carry clearing; canonical collection-helper preference)
- [ ] For each drifted pair: either document the divergence as intended
      (comment + keep separate) or fix it as a separately-gated behavior
      commit with its own pinning test — never merge-and-hope
- [ ] Milestone gate: full corpora name-identical (plus any deliberate,
      pinned fixes called out per-commit)

Step 3 — explicit pipeline:
- [ ] Order the phase calls as a single visible sequence over the state
      struct; document any genuinely dynamic ordering instead of
      flattening it
- [ ] Update the pin-holder tests to pin the pipeline shape (the phase
      sequence) rather than incidental block internals, keeping them as
      structure guards
- [ ] Milestone gate: full corpora name-identical
- [ ] Update this document's status header and note completion in
      `docs/todo.md` conventions if applicable

Definition of done: `rewriteExpr` is a short driver over named phases;
no test-visible behavior change across both corpora; the phase table in
this document matches the code; golden pins guard the new structure.

## Phase Table (Step 0 deliverable)

Derived at branch head `78407d5` from
`src/semantics/TemplateMonomorphExpressionRewrite.h` (3,440 lines;
`rewriteExpr` spans `:35-3440`). Line anchors drift with edits — re-derive
before each extraction commit; row identity is by trigger + behavior, not
line number. "Terminal" means the row returns without falling through to
the tail recursion (R21).

Every line of `rewriteExpr` is owned by exactly one row below. Rows
marked **[spelling-consumer]** read intermediate (pre-canonicalization)
spellings mid-rewrite and must not be reordered relative to the rows
that produce those spellings.

| Row | Lines | What it does | Trigger | Terminal? |
|---|---|---|---|---|
| R1 | :44 | Stamp `expr.namespacePrefix` | always | no |
| R2 | :45-47 | `rewriteTransforms` on `expr.transforms` | always | error only |
| R3 | :48-72 | `ct_if`: rewrite predicate via `rewriteCompileTimePredicateExpr`, then each branch's `bodyArguments` recursively with copied `branchLocals` | Call named `ct_if`, 3 args | **yes** |
| R4 | :73-175 | Bare-name → zero-arg call promotion; ambiguity diagnostics (`"ambiguous bare name"`); reads `ctx.sourceDefs`, `ctx.helperOverloads`, scoped import aliases, `locals`, `params`; on unique target replaces `expr` with a Call and falls into the call pipeline | `kind == Name` | no |
| R5 | :176-178 | Early `return true` for non-calls | `kind != Call` | **yes** |
| R6 | :179-229 | Lambda: recurse args + bodyArguments with lambda-scoped mapping/allowed/locals | `expr.isLambda` | **yes** |
| R7 | :231-243 | Split inline `base<args>` spelling into `name` + `templateArgs` (`"invalid template arguments"`) | `templateArgs` empty and name parses as templated | error only |
| R8 | :245-330 | Lambda definitions for R13: `resolvePickSumDefinition`, `appendPickPayloadLocal`, `recordBodyBindingLocal` | — (definitions) | — |
| R9 | :332-503 | Tuple-index early rewrite: deep self-traversal finds builtin array access with tuple-typed receiver + literal index, rewrites to `/std/tuple/get(_ref)` and re-enters `rewriteExpr`; `"tuple index must be a compile-time integer"` | scoped block, always entered | yes if handled |
| R10 | :505-594 | Type-pack spread expansion over `expr.args` and `expr.bodyArguments` (generated pack field names; reads `findBinding(params, locals, ...)`) | `ctx.currentRewriteDefinition` has `templatePackBindings` | no |
| R11 | :596-678 | Multi-`wait` → tuple: recurse args, require Task-typed handles, `instantiateTemplate("/std/tuple/tuple")`, rewrite to brace ctor | direct call `wait`, >1 args, no body/template args | **yes** |
| R12 | :680-766 | `make_tuple`: recurse args, infer/resolve template args, `instantiateTemplate`, rewrite to brace ctor | `resolveCalleePath == /std/tuple/make_tuple` | **yes** |
| R13 | :768-831 | `pick`: recurse args; sum-variant payload locals via R8; rewrite arm transforms and arm bodies with `armLocals` | `isPickCall(expr)` | **yes** |
| R14 | :833-1866 | Collection-helper lambda battery (~25 lambdas + 5 `std::function` forward decls): receiver family inference, `preferCanonicalStdlibCollectionHelperPath` (:1444), `shouldDeferStdlibCollectionHelperTemplateRewrite` (:1699), nested experimental ctor rewriters | — (definitions; consumed by R15, R16, R19, R20) | — |
| R15 | :1868-1875 | Binding-initializer ctor canonicalization (vector then key-value) via R14 `std::function`s | `expr.isBinding` | error only |
| R16 | :1876-2000 | Non-binding pre-dispatch normalization: (a) primitive-receiver method-style sugar rewrites `expr.name` to math method path; (b) string `at`/`at_unsafe` method → direct-call flip; (c) **[spelling-consumer]** helper-return SOA `get`/`ref` surface rejection (Mechanism B: `soaUnavailableMethodDiagnostic`, `"argument count mismatch for builtin"`, `"requires integer index"`) — reads bare `soa/…` and compat SOA spellings before canonicalization | `!expr.isBinding` | error only |
| R17 | :2001-2015 | `resolveTemplateArgumentList` over `expr.templateArgs`; computes `allConcrete` (threaded into R19/R20) | not `pack_at`/`typeof` intrinsic | error only |
| R18 | :2016-2079 | `pack_at` intrinsic rewrite to generated pack field access; pinned diagnostics (`"pack_at requires …"`, `"index out of range"`) | `pack_at` intrinsic | **yes** |
| R19 | :2080-3047 | Direct-call resolution cascade (sub-phases a-i below) | `!isMethodCall && !isBinding` | no (falls through) |
| R20 | :3048-3396 | Method-call resolution cascade (twin of R19; see below) | `expr.isMethodCall` | no (falls through) |
| R21 | :3398-3421 | Tail recursion: `expr.args` with `locals`; `expr.bodyArguments` with accumulating `bodyLocals` | always (surviving calls) | no |
| R22 | :3422-3438 | Builtin array-access namespace scrub (named-arg-aware receiver lookup) | direct call with builtin access name | no |
| R23 | :3439 | `return true` | always | — |

R19 sub-phases (direct-call cascade), in order:

- **R19a** :2081-2100 — nested experimental ctor rewrites on the receiver
  argument (via R14 `std::function`s).
- **R19b** :2101-2122 — `resolveCalleePath` into `resolvedPath`;
  `ctx.requirementOverloadSelectionError` check (Phase 1 diagnostics
  surface here).
- **R19c** :2123-2315 — classifier-backed canonicalization
  (`preferredCollectionHelperPath`); mismatch writes `expr.name`.
- **R19d** :2316-2582 — borrowed-SOA wrapper preference, removed
  key-value compat rejections (Mechanism C), experimental KV/vector
  temporary rewrites.
- **R19e** :2583-2666 — receiver template-arg inference for canonical
  KV/vector/SOA paths.
- **R19f** :2667-2767 — template-arg validity checks; **[spelling-consumer]**
  same-path SOA helper preference (`preferredConcreteSamePathSoaHelperPath`,
  reads bare `/soa/*` same-path spellings); helper-overload
  public→internal selection via `selectRequirementAwareHelperOverloadPath`
  (`ctx.helperOverloads`; Phase 1 type-based selection lives behind this).
- **R19g** :2768-2853 — public SOA mutator/value helper base-path
  rewrites.
- **R19h** :2854-2980 — template instantiation (`instantiateTemplate` →
  `expr.name = specializedPath`, `templateArgs.clear()`) or non-template
  finalization; implicit template-tail inference.
- **R19i** :2981-3046 — `expr.resolvedCallPath` settling, then inline
  experimental ctor argument rewrites against the resolved definition.

R20 mirrors R19's shape with method-specific entry
(`resolveMethodCallTemplateTarget` :3059), its own canonicalization
(`preferredCollectionHelperMethodPath` :3073), experimental
vector/SOA method rewrites, template instantiation (:3288), a
`resolvedCallPath` fallback chain (:3364-3372), and the twin
`rewriteExperimentalConstructorArgsForTarget` calls (:3373-3395). The
Step 2 twin-diff classifies each R19/R20 sub-phase pair.

Behavioral notes the extraction must preserve:

- R3 returns before the branch argument expressions themselves are
  rewritten — only their `bodyArguments` are; the
  compile-time-specialized branch-pruning pass consumes the un-rewritten
  branch spellings downstream.
- R4 mutates `expr.kind` from Name to Call mid-function; every later row
  assumes Call.
- R19 and R20 do NOT return true on success — their mutations feed the
  shared tail (R21/R22). Early `return false` sites are pervasive
  (49 across both cascades).
- The 5-test SOA gauntlet and the two pin-holder goldens (see Gate
  Policy) pin R16c, R19c, R19f, and R20's canonicalization ordering.

Flagged rows (dependencies not statable with confidence; targeted
experiment attached — run these during Step 1, before moving the row):

- **R9 vs R10**: does tuple-index detection depend on spread arguments
  already being expanded (R10 runs after R9)? Experiment: fixture with a
  type-pack spread argument used as a tuple-index receiver; if it
  resolves today, order is load-bearing as-is; if it errors, the order
  R9→R10 is safe to keep but not to swap.
- **R19c vs R19f ordering**: classifier canonicalization runs before
  same-path SOA preference; the consolidation doc says bare `/soa/*`
  must survive R19c untouched (resolution-stage prefix list excludes
  `/soa`). Experiment already exists as the SOA gauntlet — re-run it on
  any commit that touches either sub-phase.
- **R21 after R19/R20**: nested experimental ctor args are rewritten
  twice (R19i/R20 tail against the resolved def, then generic R21
  recursion). Experiment: fixture with an experimental KV ctor as an
  argument to a plain user helper (bypasses R19i) to confirm R21 alone
  handles it; determines whether R19i's arg rewrites are moveable into
  R21 or must stay pre-tail.

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
