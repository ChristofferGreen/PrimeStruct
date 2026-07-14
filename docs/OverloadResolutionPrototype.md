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
  signatures, the call is a compile-time diagnostic: ambiguous call

Open question, not resolved here: an unbound generic parameter (`<T>`)
structurally matches any concrete argument type, so a generic candidate in
an arity group can always survive alongside a concrete-typed candidate,
making every such pair ambiguous unless a preference rule breaks the tie
(e.g. "an exact concrete-type match wins over a generic match in the same
slot"). This needs to be settled before implementation; it is the main
semantic risk in this proposal, not the mechanical plumbing.

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

- [ ] settle the generic-vs-concrete overlap/preference rule (open question
  above) before writing any code
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
