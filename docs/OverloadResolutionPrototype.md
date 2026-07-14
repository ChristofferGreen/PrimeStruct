# PrimeStruct Type-Based Overload Resolution Prototype

Status: prototype design note, not yet part of the canonical language spec.

This document proposes relaxing the current arity-only "helper overloading"
rule so that same-path, same-arity definitions can coexist when their
parameter types differ. It is required substrate for the `Space` prototype's
`insert(string, ...)` / `insert(PathKey, ...)` design (see the in-progress
`Space` discussion; a `SpacePrototype.md` has not been written yet). When the
design here is stable enough, fold the accepted parts into
`docs/PrimeStruct.md` alongside implementation TODOs.

Before any type-based matching is added, this document now also scopes a
prerequisite refactor: overload resolution has no single source of truth
today, and that has to be fixed first. See "Prerequisite: Consolidate
Resolution Into A Single Source" below.

## Current Behavior (baseline)

Overload resolution today is arity-only and lives inside the
template-monomorphization pass, not a dedicated resolver:

- `TemplateMonomorphSourceDefinitionSetup.h:34-217`
  (`initializeTemplateMonomorphSourceDefinitions`) groups definitions by
  literal `fullPath`. For non-sum, non-struct families with more than one
  definition, it counts `parameterCountFrequencies`
  (`TemplateMonomorphSourceDefinitionSetup.h:130-157`): two definitions
  sharing both path and arity are rejected as `"duplicate definition: " +
  publicPath` unless disambiguated by a `require` transform.
- Surviving definitions get their `fullPath` rewritten to
  `helperOverloadInternalPath(path, arity)`, i.e. `path + "__ov" + arity`
  (`TemplateMonomorphCoreUtilities.h:204-206`), and are recorded in
  `ctx.helperOverloads`.
- Call-site candidate selection matches `expr.args.size()` against the
  stored `parameterCount` (`TemplateMonomorphCoreUtilities.h:561-742`, loop
  at 707-741), falling back to variadic entries.
- A parallel arity-keyed lookup exists in the general validator:
  `SemanticsValidatorExprCallResolution.cpp:178-197`
  (`overloadCandidatePath`, builds `path + "__ov" + N`) and `:513-531`
  (`buildBaseCandidates`), backed by `overloadFamilyBasePaths_` built in
  `SemanticsValidatorBuildCallResolution.cpp:10-57`.

Parameter *types* play no role in candidate selection today. Two
definitions with the same arity are always duplicates regardless of their
parameter types, unless one carries a `require<...>` guard.

## Prerequisite: Consolidate Resolution Into A Single Source

Resolution is not implemented once today — it is independently
reconstructed by string convention at every consumption site. The
`path + "__ov"` prefix is rebuilt and re-parsed via ad hoc
concatenation/`rfind` prefix-matching in at least:

- `SemanticsValidatorExprCallResolution.cpp` alone, four separate times:
  `:45` (`overloadPrefix = resolvedPath + "__ov"`), `:146-157` (a third,
  differently cached, local helper that rebuilds `path + "__ov"`), `:194`
  (`overloadPrefix = rawPath + "__ov"` again), and `:480`/`:489`
  (`rfind(canonicalPath + "__ov", 0) == 0`, twice, for different path
  variants)
- 18 files total under `src/semantics/*`
- 10 files under `src/ir_lowerer/*` — `IrLowererCallResolution.cpp`,
  `IrLowererStatementCallEmission.cpp`,
  `IrLowererStatementBindingStatementEmit.cpp`,
  `IrLowererSetupTypeMethodCallResolution.cpp`,
  `IrLowererLowerSumHelpers.h`, `IrLowererLowerStatementsExpr.h`,
  `IrLowererLowerStatementsBindings.h`,
  `IrLowererLowerEmitExprTailDispatch.h`,
  `IrLowererLowerEmitExprCollectionHelpers.h`,
  `IrLowererBindingTypeHelpers.cpp` — each re-parsing `__ov` for its own
  purpose (call resolution, binding emission, collection-helper rewriting,
  sum-helper lowering) instead of consuming an already-resolved reference

No single function decides "which definition does this call resolve to"
and publishes that answer once. Every consumer re-derives it by agreeing,
informally, on what a magic string substring means. This contradicts
PrimeStruct's own stated architecture: the whole language is built around
flowing decisions through one canonical form (one IR, a semantic product
that publishes facts for downstream stages to consume) rather than letting
each stage re-derive the same answer independently. Extending the current
string convention with a second dimension — a type-signature hash, as
"Compiler Surface Impacted" below originally proposed — would only add
that dimension to all 28+ sites instead of fixing the duplication.

**Refactor shape:**

- introduce one canonical resolution function/pass that, given a call
  site, returns a single resolved definition reference (a strongly-typed
  ID or pointer, not a path string)
- attach that reference directly to the call's AST/semantic-product node
  at resolution time
- every downstream consumer — the 10 `ir_lowerer` files and every
  `semantics` file that currently reconstructs `path + "__ov"` for its own
  candidate search — reads the attached reference instead of re-deriving
  it
- the `__ov<N>` path suffix becomes a debug/display/generated-symbol
  naming detail only, not something any consumer parses to make a decision
- this refactor must be behavior-preserving on its own: same arity-only
  resolution rules, same diagnostics, verified by the existing overload
  test suite before any type-based matching is added

**Sequencing:** this consolidation is step zero, ahead of everything else
in this document.

1. Consolidate arity-only resolution to one source of truth (behavior
   preserving; existing tests must pass unchanged)
2. Only then extend that single source to also match on parameter type
   (the rest of this document)

### Phase 0 Progress

Implemented and verified (exact test-name diff of the full semantics suite,
2923 cases, between an unmodified baseline and the modified tree: identical
178 pre-existing failures, zero regression):

- `Expr::resolvedCallPath` (`include/primec/Ast.h`), populated once during
  `TemplateMonomorphization` for both direct and method calls
  (`TemplateMonomorphExpressionRewrite.h:2978-2984`, `:3360-3372`), at the
  exact point the existing rewrite logic already trusts its own
  `resolveCalleePath` result for its own subsequent purposes — not a new
  resolution decision, just capturing an existing one
  - confirmed unmutated by every later pass before IR lowering consumes it
    (`CompileTimeSpecializedBranchPruning`, `ReflectionMetadataQueries`,
    `OmittedStructInitializers`, `SemanticNodeIdAssignment`,
    `SemanticProductPublication` — all either don't touch resolved call
    nodes or are read-only by construction)
  - one real staleness bug found and fixed: `ConvertConstructors`
    (`SemanticsValidateConvertConstructors.cpp`) retargets `convert<T>(...)`
    calls after monomorphization already captured the field; now kept in
    sync at the retarget site
- 5 validator call sites migrated to prefer the field over re-deriving via
  plain `resolveCalleePath(expr)` with no other resolver layered in front
  (`SemanticsValidatorStatementReturns.cpp` x2,
  `SemanticsValidatorExprMutationBorrows.cpp` x1,
  `SemanticsValidatorInferCollectionCallResolution.cpp` x2)

**Blocked, with concrete evidence, not just caution:** a third resolver,
`SemanticsValidator::preferredCollectionHelperResolvedPath`
(`SemanticsValidatorBuildInitializerInference.cpp:104-216`, used at 33 call
sites and ahead of `resolveCalleePath` in the publication step that feeds
`ir_lowerer`'s own semantic-product-trusting fast path), is **not**
equivalent to `resolvedCallPath`. A differential audit — instrumenting the
function to compare its result against `expr.resolvedCallPath` on every
call, then running the full semantics and `ir_pipeline` test corpora —
found real divergence: legacy-spelling SOA helper calls (`/soa/get`,
`/soa/ref`) resolve to their literal compat-spelling definition via
`resolveCalleePath` (and therefore `resolvedCallPath`), while
`preferredCollectionHelperResolvedPath` deliberately forces the canonical
`/std/collections/soa/get`/`/ref` path whenever that canonical definition
exists — regardless of whether the compat spelling also has its own literal
definition. This is intentional canonicalization in the validator that
monomorphization's `resolveCalleePath` does not fully replicate today.

This means the remaining Phase 0 scope (the rest of the validator's
`preferredCollectionHelperResolvedPath`-first sites, the publication step,
and all 10 `src/ir_lowerer/*` files) cannot be safely migrated by simply
trusting `resolvedCallPath` — the two resolvers have different
canonicalization goals for at least the SOA compat-spelling family, and
possibly others not exercised by the current test corpus. The actual
follow-up is to extend monomorphization's own SOA/vector/key-value
compat-path canonicalization (already partially present via
`preferVectorStdlibHelperPath`/`preferVectorStdlibTemplatePath`,
`TemplateMonomorphCollectionCompatibilityPaths.h:95-162`) to match
`preferredCollectionHelperResolvedPath`'s canonicalization exactly, so that
`resolvedCallPath` always holds the canonical path — at which point the
remaining validator sites, the publication step, and `ir_lowerer` can all
migrate safely. That is new resolver work, not a read-instead-of-rederive
substitution, and is unstarted.

**Attempted and reverted:** a first attempt added a narrow canonicalization
step to monomorphization's `resolveCalleePath`
(`TemplateMonomorphTypeResolution.h`) that mapped bare public-surface SOA
spellings (`/soa/get`, `/soa/ref`, ...) to their stdlib target, gated on no
literal definition already existing at the bare path (to preserve
user-defined same-path shadows, e.g. a user's own `/soa/ref` overriding the
stdlib helper — the first version of this attempt broke exactly that case
before the gate was added). Even with the gate, differential testing found
further regressions: several tests expect bare `/soa/push`, `/soa/reserve`,
and similar retired/removed compat spellings to be explicitly rejected with
"unknown method" diagnostics in certain method-call contexts
(`isRemovedVectorCompatibilityHelper`/`isRemovedBorrowedSoaCompatibilityHelper`,
`TemplateMonomorphCollectionCompatibilityPaths.h:4-13`) — the new
canonicalization step resolved them successfully instead, since it ran
without checking removed-helper status first. A full-suite verification run
also stalled for an extended period on this build without producing new
output, which was not conclusively diagnosed as a hang before the change
was reverted; it remains an open question whether that was a genuine
pathological case introduced by this change or environmental slowness (this
session saw multiple full-suite runs take 2-3x longer than earlier runs
with no code changes in between). The change was reverted in full
(`TemplateMonomorphTypeResolution.h` restored to its pre-attempt state,
confirmed via empty `git diff`) rather than layering more special-case
guards onto it reactively.

**Conclusion:** `preferredCollectionHelperResolvedPath`'s SOA branch
encodes more than a simple compat-to-canonical rename — it interacts with
removed-helper rejection and possibly other rules not yet enumerated.
Replicating it correctly in monomorphization needs the same rule set
mapped out deliberately (what counts as removed, in which call shapes,
interacting with which other checks) before another attempt, not another
incremental patch. Until then, Phase 0 stays at the state verified safe by
exact test-name diff: the `resolvedCallPath` field, the `convert<T>` fix,
and the 5 plain-`resolveCalleePath` validator sites from earlier in this
document. The remaining validator sites, the publication step, and all 10
`ir_lowerer` files remain unmigrated.

## Design Goals

- allow two or more definitions at the same public path with the same
  parameter count to coexist when their parameter types differ
- call sites resolve to the definition whose parameter types match the
  argument types, at compile time
- ambiguous matches remain a compile-time diagnostic, never a runtime choice
- extend rather than replace: arity stays the first partitioning filter,
  exactly as today; type matching is a second filter applied only within an
  arity group that has more than one surviving candidate
- keep the first-slice matching rule simple: exact parameter type equality,
  not a coercion/conversion ranking pass

## Non-Goals For The First Slice

- implicit numeric coercion or promotion-based overload ranking
- C++-style "best viable candidate" scoring with conversion sequences
- overloading on return type alone
- a unified arity+type scoring pass — arity partitioning stays first,
  unchanged; type matching only decides within one arity group
- new variadic/pack-parameter overload participation beyond what already
  exists
- runtime/dynamic dispatch of any kind. This is purely a compile-time
  call-site resolution feature. A `Space` mount-boundary trait object still
  needs its own dynamic dispatch story regardless of this feature.

## Proposed Behavior

Within one `(path, arity)` group — the existing first-stage filter, kept
as-is — partition further by parameter type signature:

- a candidate survives if every parameter's declared type is exactly equal
  to the corresponding argument's static type, using the same type
  inference already applied uniformly to that argument (e.g. literal-suffix
  inference); no candidate-specific coercion is attempted
- if exactly one candidate survives, the call resolves to it
- if zero candidates survive, the call is a compile-time diagnostic: no
  viable overload
- if more than one candidate has an *identical* parameter-type signature at
  the same path and arity, that is still the existing duplicate-definition
  diagnostic, now scoped to `(path, arity, type signature)` instead of just
  `(path, arity)`
- if more than one candidate survives with *different* viable type
  signatures, apply the specificity rule below before diagnosing ambiguity

### Specificity Rule (Partial Ordering, C++-Style)

An unbound generic parameter (`<T>`) structurally matches any argument
type, so a generic candidate in an arity group is always viable alongside
a concrete-typed candidate. Left unresolved, every such pair would be
"ambiguous," which defeats the point (a generic fallback next to a
type-specific override is a normal, expected pattern — it's how the
existing `/helper/value<T>` example in `docs/PrimeStruct.md` already
reads, just currently separated by arity instead of type).

Resolve it the way C++ does, taking only the two rules that apply here and
explicitly leaving out conversion-sequence ranking (already excluded under
Non-Goals — viability itself stays exact-type-only, this rule only breaks
ties among already-viable candidates):

1. **Concrete beats generic.** A candidate with no unbound generic
   parameters beats any viable candidate that has one or more unbound
   generic parameters.
2. **Partial ordering among generic candidates.** When two viable
   candidates both have unbound generic parameters, compare them
   parameter-by-parameter. In a given position: a concrete type is more
   specific than an unbound generic parameter; two concrete types are
   equally specific (and, since viability already required an exact
   match, they are the same type); two unbound generic parameters are
   equally specific. Candidate `F` is more specific than candidate `G` if
   `F` is at least as specific as `G` in every position and strictly more
   specific in at least one. If exactly one viable candidate is more
   specific than every other viable candidate, it wins.
3. **Incomparable stays ambiguous.** If no viable candidate is more
   specific than every other — two candidates are each more specific than
   the other in different parameter positions — the call is a compile-time
   diagnostic: ambiguous call. This is a real, intended outcome, not a gap
   to patch over with an arbitrary tie-break; PrimeStruct does not invent
   an ordering beyond specificity.

Example, rule 1 (concrete beats generic):

```prime
[T] insert([string] path, [T] value) { ... }
[A, T] insert([A] path, [T] value) { ... }
```

```prime
space.insert("/sys/data", 5)   // resolves to the [string] candidate
```

Example, rule 3 (genuinely ambiguous — neither dominates):

```prime
[A] handle([A] first, [string] second) { ... }
[A] handle([string] first, [A] second) { ... }
```

```prime
handle("x", "y")
```

```text
ambiguous call to `handle`: candidates (A, string) and (string, A) are
each more specific than the other in one parameter for (string, string)
```

## Example

```prime
[T] insert([string] path, [T] value) {
  return(insert(hash_path(path), value))
}

[T] insert([PathKey] key, [T] value) {
  // real storage/routing logic
}
```

```prime
space.insert("/sys/data", 5)   // resolves to the string overload
space.insert(cached_key, 5)    // resolves to the PathKey overload
```

Both definitions share path `insert` and arity 2. Under today's rule this
is a duplicate-definition error; under this proposal it resolves by
parameter type.

## Diagnostics

Expected diagnostic shapes:

```text
no viable overload for `insert`: no candidate accepts (bool, i32)
ambiguous call to `insert`: candidates (string, T) and (PathKey, T) both viable for (string, i32)
ambiguous call to `handle`: candidates (A, string) and (string, A) are incomparable for (string, string)
duplicate definition: insert(string, T) already declared with this exact signature
```

Diagnostics should name the call site, the argument types actually supplied,
and every candidate signature considered.

## Compiler Surface Impacted

Today, the ~28 files enumerated under "Prerequisite" above are each a
direct dependency, because each one independently re-parses the `__ov`
convention. That is precisely the problem Phase 0 removes.

After Phase 0 lands, the internal overload key growing from
`path + "__ov" + arity` to something that also encodes the parameter type
signature — e.g. `path + "__ov" + arity + "__ty" + typeSignatureHash` — is
a change confined to the one canonical resolution function, plus:

- `TemplateMonomorphSourceDefinitionSetup.h` and
  `TemplateMonomorphFinalOrchestration.h:379-410` (definition grouping and
  path rewrite, which still needs to run once per compilation to assign the
  keys in the first place)
- duplicate-definition diagnostics:
  `SemanticsValidatorDiagnostics.cpp:189-273`,
  `SemanticsValidatorBuild.cpp:208-212`
- `src/StdlibSurfaceRegistry.cpp`

Encoding a stable hash of a type-name sequence into that key is the same
class of problem as the compile-time string-hashing feature discussed for
`Space` path segments — worth reusing that mechanism here instead of
inventing a second one, once it exists.

No `SOURCE_LOCK`-style marker gates this behavior today, and the
`docs/PrimeStruct.md` overload example is not spec-tested, so nothing
formally blocks either phase — but Phase 0 (consolidation) is the
medium-large cross-cutting change (~28 files, each touched once to remove
its private `__ov` parsing); Phase 1 (type-based matching) becomes a
small, localized change once Phase 0 is done.

## Interaction With `require`-Constrained Overloads

The existing `require<...>` escape hatch (TODO-4553) already lets same-arity
definitions coexist when guarded by a compile-time requirement predicate
(e.g. constraining `T` to a trait). Type-based resolution does not replace
this: `require<...>` still expresses constraints beyond raw parameter type
identity, such as "T is Additive." The two mechanisms should coexist —
type-signature matching handles the simple case (concrete types differ, as
in `string` vs `PathKey`) without forcing a manual `require` guard for it.

## Backward Compatibility

Programs that previously received a "duplicate definition" diagnostic for
same-arity, different-parameter-type definitions will newly compile. This
is diagnostic-narrowing, not diagnostic-widening: strictly more programs
become accepted, none become rejected. Existing tests asserting "duplicate
same-arity definitions are rejected" (e.g. "templated helper overload
families resolve by exact arity," "...still reject duplicate same-arity
definitions," "generic sum overload families resolve by template arity,"
"requirement constrained overload selects the only viable candidate" — see
`tests/TEST_INVENTORY.md`) need to be re-scoped to "duplicate same-arity
*same-type* definitions are rejected," and new positive/negative tests are
needed for the type-differentiated case.

## Relationship To Space

This is the dependency the `Space.insert` design needs to become a true
overload set (`insert(string, ...)` and `insert(PathKey, ...)` under one
name) instead of two differently-named helpers. Until this substrate lands,
`Space` should ship with the workaround: `insert` (string) calling
`insert_by_key` (PathKey) internally, which requires no new language
capability.

## First Implementation Checklist

Phase 0 — consolidate before extending:

- [ ] introduce one canonical resolution function/pass producing a single
  resolved definition reference per call site
- [ ] attach that reference to the call's AST/semantic-product node
- [ ] migrate `SemanticsValidatorExprCallResolution.cpp`'s four independent
  `path + "__ov"` reconstructions (`:45`, `:146-157`, `:194`, `:480`/`:489`)
  and the rest of the 18 `src/semantics/*` sites to read the attached
  reference instead of rebuilding/re-parsing the string convention
- [ ] migrate all 10 `src/ir_lowerer/*` sites to read the attached
  reference instead of parsing `__ov`
- [ ] verify behavior-preserving: existing arity-only overload tests pass
  unchanged with no diagnostic or resolution-outcome differences

Phase 1 — add type-based matching on top of the consolidated source:

- [ ] implement the specificity/partial-ordering tie-break rule (see
  "Specificity Rule (Partial Ordering, C++-Style)" above), including the
  incomparable-stays-ambiguous diagnostic
- [ ] extend the single canonical resolution function's grouping key from
  `(path, arity)` to `(path, arity, parameter-type-signature)`
- [ ] extend the internal path/key scheme beyond `__ov<N>` to also encode a
  type signature, in the one place that now owns it
- [ ] update the canonical resolution function to filter by argument
  types, not just count
- [ ] update `src/StdlibSurfaceRegistry.cpp`
- [ ] add "no viable overload" and "ambiguous call" diagnostics, distinct
  from the existing duplicate-definition diagnostic
- [ ] re-scope existing arity-only overload tests; add new type-based
  positive and negative tests
- [ ] update `docs/PrimeStruct.md`'s "Helper overloading" section only
  after the supported surface is implemented
