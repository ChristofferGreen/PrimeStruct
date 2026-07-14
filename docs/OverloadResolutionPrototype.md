# PrimeStruct Type-Based Overload Resolution Prototype

Status: prototype design note, not yet part of the canonical language spec.

This document proposes relaxing the current arity-only "helper overloading"
rule so that same-path, same-arity definitions can coexist when their
parameter types differ. It is required substrate for the `Space` prototype's
`insert(string, ...)` / `insert(PathKey, ...)` design (see the in-progress
`Space` discussion; a `SpacePrototype.md` has not been written yet). When the
design here is stable enough, fold the accepted parts into
`docs/PrimeStruct.md` alongside implementation TODOs.

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

The internal overload key must grow from `path + "__ov" + arity` to
something that also encodes the parameter type signature, e.g.
`path + "__ov" + arity + "__ty" + typeSignatureHash`. Encoding a stable
hash of a type-name sequence into that key is the same class of problem as
the compile-time string-hashing feature discussed for `Space` path
segments — worth reusing that mechanism here instead of inventing a second
one, once it exists.

That key change is cross-cutting:

- monomorphization grouping and path rewrite:
  `TemplateMonomorphSourceDefinitionSetup.h`,
  `TemplateMonomorphCoreUtilities.h` (path rewrite at 204-206, candidate
  loop at 707-741), `TemplateMonomorphFinalOrchestration.h:379-410`
  (`rewriteMonomorphizedDefinitions` /
  `resolveHelperOverloadDefinitionIdentity` at
  `TemplateMonomorphCoreUtilities.h:745-774`)
- general validator candidate lookup:
  `SemanticsValidatorExprCallResolution.cpp:178-197,513-531`,
  `SemanticsValidatorBuildCallResolution.cpp:10-57`
- duplicate-definition diagnostics:
  `SemanticsValidatorDiagnostics.cpp:189-273`, `SemanticsValidatorBuild.cpp:208-212`
- IR lowering: 12 files under `src/ir_lowerer/*` currently parse the
  `__ov<N>` suffix for call resolution, binding emission, collection-helper
  rewriting, and sum-helper lowering; each needs to carry/parse the type
  signature component too
- `src/StdlibSurfaceRegistry.cpp`

No `SOURCE_LOCK`-style marker gates this behavior today, and the
`docs/PrimeStruct.md` overload example is not spec-tested, so nothing
formally blocks the change — but the blast radius above (28 files total)
makes this a medium-large cross-cutting change, not a local patch.

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

- [ ] settle the generic-vs-concrete overlap/preference rule (open question
  above) before writing any code
- [ ] extend monomorphization grouping key from `(path, arity)` to
  `(path, arity, parameter-type-signature)` in
  `TemplateMonomorphSourceDefinitionSetup.h`
- [ ] extend the internal path/key scheme beyond `__ov<N>` to also encode a
  type signature, in `TemplateMonomorphCoreUtilities.h`
- [ ] update call-site candidate selection in
  `TemplateMonomorphCoreUtilities.h` and
  `SemanticsValidatorExprCallResolution.cpp` /
  `SemanticsValidatorBuildCallResolution.cpp` to filter by argument types,
  not just count
- [ ] propagate the new key scheme through the 12 `src/ir_lowerer/*` files
  that parse `__ov`
- [ ] update `src/StdlibSurfaceRegistry.cpp`
- [ ] add "no viable overload" and "ambiguous call" diagnostics, distinct
  from the existing duplicate-definition diagnostic
- [ ] re-scope existing arity-only overload tests; add new type-based
  positive and negative tests
- [ ] update `docs/PrimeStruct.md`'s "Helper overloading" section only
  after the supported surface is implemented
