# Collection Decoupling: Moving Collection Knowledge from C++ to .prime

Status: Proposed

## Problem

The PrimeStruct compiler has ~75 production source files with hardcoded
knowledge of collection data structures (vector, map, soa). This couples
the compiler to specific stdlib types and makes it harder to evolve
collections independently of the compiler.

The goal: collections should be entirely defined in `.prime` files. The
C++ compiler/emitter code should have no special-cased knowledge of
vector, map, or soa.

## Key Architectural Fact

The IR (PSIR v22) has **zero collection-specific opcodes**. All collection
operations lower to generic memory/call IR (`LoadLocal`, `StoreLocal`,
`HeapAlloc`, etc.). The `.prime` stdlib files already ARE the
implementation for all backends (VM, native, C++ emitter). The hardcoded
C++ code is purely **routing/dispatch**, not implementation.

## Current State

| Area | Files | Deep logic | Surface-only |
|------|-------|------------|--------------|
| Semantics | 35+ | 30+ | 5 |
| IR Lowerer | 18+ | 16+ | 2 |
| Emitter | 20+ | 18+ | 2 |
| Include | 6 | 4 | 2 |
| Runtime | 0 | 0 | 0 |

### Categories of Hardcoded Knowledge

**1. Method dispatch (~3765 lines)**
`SemanticsValidatorExprMethodTargetResolution.cpp` resolves
`vec.push(x)` to `/std/collections/vector/push` via hardcoded
if/else chains. The `surfaces.psmeta` manifest already declares
member names but the routing logic is in C++.

**2. Type recognition (61 call sites, 20 files)**
`isVectorTypeName()`, `isMapTypeName()`, `isKeyValueCollectionTypeName()`
checks scattered across semantics, IR lowerer, and emitter. Controls
return type inference, compatibility checking, slot layout, and effect
annotation inference.

**3. Builtin helper name aliases (~786 lines)**
`IrLowererBuiltinNameHelpers.cpp` maps bare names (`count`, `push`,
`at`) to canonical paths. The `_ref` suffix routing (borrowed receiver
variants) and compatibility spelling routing are hardcoded.

**4. Struct slot layout (~1234 lines)**
`IrLowererStructSlotLayoutHelpers.cpp` branches on
`isVectorTypeName()` / `isMapTypeName()` to decide memory layout
for the native backend. The `.prime` struct declarations already
contain enough field information.

**5. SOA rewrites (~200 references)**
Dedicated routing for `to_aos`, `field_view`, `soa_vector` path
construction across 15+ files. The `.prime` files contain the full
implementation; C++ only handles dispatch. Also includes
`ContainerError` paths in 12+ files.

**6. C++ emitter helpers (already solved)**
The emitter does NOT emit collection-specific C++ helpers. The
`.prime` definitions are the implementation for all backends.

## Feasibility Assessment

| Category | Gap Size | Existing Infrastructure | Feasible? |
|----------|----------|------------------------|-----------|
| Method dispatch | Small | `surfaces.psmeta` manifest | Yes |
| Type recognition | Medium | `has_trait` predicate system | Yes, with trait mechanism |
| Name aliases | Very small | Manifest + `StdlibSurfaceMemberAlias` | Yes |
| Slot layout | Medium | `.prime` struct field declarations | Yes, refactoring |
| SOA rewrites | Small-medium | Manifest SOA entries | Yes |
| ContainerError | Small | Existing `CollectionsContainerErrorHelpers` entry | Yes |
| C++ emitter | None | Already done | N/A |

### Task Sizing Verification

| TODO | Phase | Scope | One round? |
|------|-------|-------|------------|
| 4656 | 1 | Audit manifest coverage | Yes — investigation only |
| 4657 | 1 | Add borrowed_variant to manifest schema + one migration | Yes — schema + one site |
| 4658 | 1 | Migrate isRemovedVectorCompatibilityHelper name set (lines 17-22) | Yes — one function, 10 strings |
| 4659 | 1 | Migrate IrLowererBuiltinNameHelpers alias set (lines 87-126) | Yes — one function |
| 4660 | 1 | Migrate EmitterBuiltinCallPathHelpers alias set (lines 45-84) | Yes — one function |
| 4661 | 1 | Migrate to_aos compatibility spelling (lines 1976, 3262) | Yes — one file, one routing family |
| 4662 | 2 | Design type-category annotation spec | Yes — design doc only |
| 4663 | 2 | Implement predicate in RequirementPredicateFacts | Yes — one file, one predicate |
| 4664 | 2 | Annotate 4 stdlib types (vector, map, soa_vector, soa_column) | Yes — mechanical .prime edits |
| 4665 | 2 | Migrate SemanticsValidatorExprVectorHelpers (4 sites) | Yes — one file |
| 4666 | 2 | Migrate IrLowererStructSlotLayoutHelpers (6 calls + 1 string) | Yes — one file |
| 4667 | 2 | Migrate EmitterBuiltinCollectionInferenceHelpers | Yes — one file |
| 4668 | 3 | Audit slot layout branches | Yes — investigation only |
| 4669 | 3 | Implement generic vector slot count in normalizeVectorStructPath | Yes — one function |
| 4670 | 3 | Delete isVectorTypeName/isMapTypeName from slot layout | Yes — two function deletions |
| 4671 | Cleanup | Remove isVectorTypeName/isMapTypeName globally | Yes — grep-verified deletion |
| 4672 | 1 | Migrate isRemovedKeyValueCompatibilityHelper (lines 24-32) | Yes — one function, 13 strings |
| 4673 | 1 | Migrate method dispatch chains (lines 130, 3328-3331) | Yes — one file, two sites |
| 4674 | 1 | Migrate SOA helper routing beyond to_aos | Yes — two files |
| 4675 | 1 | Migrate ContainerError paths in IR lowerer | Yes — two files |

## Migration Phases

### Phase 1: Extend the surface manifest

The `surfaces.psmeta` manifest and `StdlibSurfaceRegistry` already
externalize member names. Extending them to cover method routing,
borrowed receiver variants, and compatibility spellings eliminates
the bulk of the hardcoded dispatch in semantics and IR lowerer.

### Phase 2: Type-category declarations

Add a mechanism for `.prime` types to declare properties (e.g.,
"this is a collection type", "this is a key-value type"). Migrate
`isVectorTypeName()` / `isMapTypeName()` call sites to use it.

### Phase 3: Generic slot layout

Refactor the native backend's slot layout resolver to compute
layout from `.prime` struct field declarations instead of
branching on known type names.

---

## TODO Items

### Phase 1: Surface Manifest Extension

- [ ] TODO-4656: Audit surfaces.psmeta manifest coverage
  - owner: ai
  - created_at: 2026-06-13
  - phase: Phase 1 - Surface Manifest Extension
  - scope: Compare the member names declared in
    `stdlib/std/collections/surfaces.psmeta` against the hardcoded
    helper name sets in     `SemanticsValidatorExprMethodTargetResolution.cpp`
    (lines 17-22, 24-32), `IrLowererBuiltinNameHelpers.cpp`
    (lines 87-126), and `EmitterBuiltinCallPathHelpers.cpp`
    (lines 45-84). Produce a gap list of names present in C++ but
    absent from the manifest.
  - acceptance:
    - Gap list document produced with exact line references
    - Each missing name categorized as: member, alias, or
      lowering_spelling
  - stop_rule: gap list complete

- [ ] TODO-4657: Add borrowed receiver variant metadata to manifest
  - owner: ai
  - created_at: 2026-06-13
  - phase: Phase 1 - Surface Manifest Extension
  - depends_on: TODO-4656
  - scope: The `_ref` suffix routing (e.g., `count` → `count_ref`,
    `at` → `at_ref`, `to_aos` → `to_aos_ref`) is hardcoded in
    `SemanticsValidatorExprMethodTargetResolution.cpp` (lines 511,
    1000, 1867, 1965) and `TemplateMonomorphExpressionRewrite.h`
    (lines 1371-1439). Add `borrowed_variant` entries to the
    manifest schema so the compiler can look up the borrowed
    receiver variant instead of hardcoding the mapping.
  - acceptance:
    - `surfaces.psmeta` declares borrowed variants for all
      applicable members
    - `StdlibSurfaceRegistry` reads and exposes the new entries
    - At least one hardcoded `count_ref` / `to_aos_ref` routing
      replaced with manifest lookup
  - stop_rule: manifest extended and one call site migrated

- [ ] TODO-4658: Migrate method target resolution helper name sets to manifest
  - owner: ai
  - created_at: 2026-06-13
  - phase: Phase 1 - Surface Manifest Extension
  - depends_on: TODO-4657
  - scope: Replace the hardcoded method name set at
    `SemanticsValidatorExprMethodTargetResolution.cpp` lines 17-22
    (`isRemovedVectorCompatibilityHelper`: "count", "capacity", "at",
    "at_unsafe", "push", "pop", "reserve", "clear", "remove_at",
    "remove_swap") with a `StdlibSurfaceRegistry` query.
  - acceptance:
    - Hardcoded set at lines 17-22 removed
    - Vector compatibility helper check uses registry query
    - Semantics tests pass
  - stop_rule: method name sets removed and tests pass

- [ ] TODO-4659: Migrate IR lowerer builtin name helpers to manifest
  - owner: ai
  - created_at: 2026-06-13
  - phase: Phase 1 - Surface Manifest Extension
  - depends_on: TODO-4657
  - scope: Replace the hardcoded alias set in
    `IrLowererBuiltinNameHelpers.cpp` (lines 87-126) with
    manifest-driven lookup. The `isNamespacedStdlibBuiltinAlias()`
    function should query the registry instead of matching against
    a hardcoded string set.
  - acceptance:
    - Hardcoded alias set at lines 87-126 removed
    - `isNamespacedStdlibBuiltinAlias()` uses registry lookup
    - IR pipeline tests pass
  - stop_rule: alias set removed and tests pass

- [ ] TODO-4660: Migrate emitter builtin call path helpers to manifest
  - owner: ai
  - created_at: 2026-06-13
  - phase: Phase 1 - Surface Manifest Extension
  - depends_on: TODO-4657
  - scope: Replace the hardcoded alias set in
    `EmitterBuiltinCallPathHelpers.cpp` (lines 45-84) with
    manifest-driven lookup.
  - acceptance:
    - Hardcoded alias set at lines 45-84 removed
    - Emitter uses registry for call path resolution
    - Compile-run tests pass
  - stop_rule: alias set removed and tests pass

- [ ] TODO-4661: Migrate SOA to_aos compatibility spelling to manifest
  - owner: ai
  - created_at: 2026-06-13
  - phase: Phase 1 - Surface Manifest Extension
  - depends_on: TODO-4657
  - scope: The compatibility spelling routing (old `soa_vector/to_aos`
    → new `soa/to_aos`, retired `soa_vector` spelling rejection) is
    hardcoded across `SemanticsValidate.cpp` (lines 965-1021,
    3835-3895, 5044-5059, 5241-5259) and
    `SemanticsBuiltinPathHelpers.cpp` (lines 931-1113). Add
    `compatibility_spelling` entries to the manifest so the
    compiler can resolve old paths via lookup.
  - acceptance:
    - Manifest declares compatibility spellings for SOA surfaces
    - At least the `to_aos` / `to_aos_ref` routing in
      `SemanticsValidate.cpp` replaced with manifest lookup
    - SOA tests pass
  - stop_rule: compatibility spellings in manifest and one
    migration complete

### Phase 2: Type-Category Declarations

- [ ] TODO-4662: Design type-category annotation syntax for .prime
  - owner: ai
  - created_at: 2026-06-13
  - phase: Phase 2 - Type-Category Declarations
  - scope: Design a `.prime` annotation syntax for declaring type
    categories (e.g., `[collection_type]`, `[key_value_type]`).
    The design must integrate with the existing `has_trait`
    predicate system in `RequirementPredicateFacts.cpp` and the
    struct transform system. Produce a spec with examples showing
    how `Vector<T>`, `Map<K,V>`, and `SoaVector<T>` would declare
    their categories.
  - acceptance:
    - Spec document with syntax examples
    - Integration points with `has_trait` identified
    - Migration path for `isVectorTypeName()` / `isMapTypeName()`
      call sites described
  - stop_rule: spec complete

- [ ] TODO-4663: Implement type-category predicate in semantic validator
  - owner: ai
  - created_at: 2026-06-13
  - phase: Phase 2 - Type-Category Declarations
  - depends_on: TODO-4662
  - scope: Implement the type-category predicate so that `.prime`
    structs annotated with the new syntax are queryable via the
    existing `has_trait` system. The predicate should replace
    direct string comparisons against type names.
  - acceptance:
    - New predicate registered in `RequirementPredicateFacts.cpp`
    - `.prime` structs with the annotation are recognized
    - At least one `isVectorTypeName()` call site replaced with
      predicate query
  - stop_rule: predicate works and one call site migrated

- [ ] TODO-4664: Annotate stdlib collection types with category declarations
  - owner: ai
  - created_at: 2026-06-13
  - phase: Phase 2 - Type-Category Declarations
  - depends_on: TODO-4663
  - scope: Add the type-category annotations to `Vector<T>` in
    `vector.prime`, `Map<K,V>` in `map.prime`, `SoaVector<T>` in
    `experimental_soa_vector.prime`, and `SoaColumn<T>` in
    `internal_soa_storage.prime`.
  - acceptance:
    - All four types annotated
    - `has_trait<T>(Collection)` returns true for annotated types
    - Existing tests pass
  - stop_rule: annotations added and tests pass

- [ ] TODO-4665: Migrate SemanticsValidatorExprVectorHelpers to predicate queries
  - owner: ai
  - created_at: 2026-06-13
  - phase: Phase 2 - Type-Category Declarations
  - depends_on: TODO-4664
  - scope: Replace `isVectorTypeName()`, `isMapTypeName()`,
    `isKeyValueCollectionTypeName()`, and
    `isExperimentalCollectionBackingTypeName()` calls across the
    20+ semantics files with predicate-based queries. Start with
    the files that have the most call sites:
    `SemanticsValidatorExprMethodTargetResolution.cpp`,
    `SemanticsValidatorExprVectorHelpers.cpp`,
    `SemanticsValidatorExprCollectionAccessValidation.cpp`,
    `SemanticsValidatorExprPreDispatchDirectCalls.cpp`.
  - acceptance:
    - At least 4 semantics files migrated to predicate queries
    - Hardcoded type name comparisons removed from migrated files
    - All semantics tests pass
  - stop_rule: 4 files migrated and tests pass

- [ ] TODO-4666: Migrate IrLowererStructSlotLayoutHelpers to predicate queries
  - owner: ai
  - created_at: 2026-06-13
  - phase: Phase 2 - Type-Category Declarations
  - depends_on: TODO-4664
  - scope: Replace `isVectorTypeName()` / `isMapTypeName()` calls
    in `IrLowererStructSlotLayoutHelpers.cpp`,
    `IrLowererSetupTypeMethodTargetHelpers.cpp`,
    `IrLowererLowerStatementsExpr.h`, and
    `IrLowererLowerInlineCalls.h` with predicate queries.
  - acceptance:
    - At least 3 IR lowerer files migrated
    - All IR pipeline tests pass
  - stop_rule: 3 files migrated and tests pass

- [ ] TODO-4667: Migrate EmitterBuiltinCollectionInferenceHelpers to predicate queries
  - owner: ai
  - created_at: 2026-06-13
  - phase: Phase 2 - Type-Category Declarations
  - depends_on: TODO-4664
  - scope: Replace `isCollectionVectorValue()`,
    `isKeyValueStorageValue()`, `isArrayValue()` calls in the
    emitter files with predicate-based queries. These functions
    in `EmitterBuiltinCollectionInferenceHelpers.cpp` (lines 55-166)
    and `EmitterHelpersTypes.cpp` (lines 78-135) currently check
    type names against hardcoded patterns.
  - acceptance:
    - At least 3 emitter files migrated
    - All compile-run tests pass
  - stop_rule: 3 files migrated and tests pass

### Phase 3: Generic Slot Layout

- [ ] TODO-4668: Audit slot layout branching for .prime struct coverage
  - owner: ai
  - created_at: 2026-06-13
  - phase: Phase 3 - Generic Slot Layout
  - scope: Read `IrLowererStructSlotLayoutHelpers.cpp` and catalog
    every `isVectorTypeName()` / `isMapTypeName()` branch. For each
    branch, determine whether the `.prime` struct's field
    declarations contain enough information to compute the same
    layout generically. Identify any branches that require
    information not present in `.prime` declarations.
  - acceptance:
    - Branch catalog with line numbers
    - For each branch: "computable from .prime" or "needs new
      .prime metadata" with justification
  - stop_rule: catalog complete

- [ ] TODO-4669: Implement generic vector slot count from .prime fields
  - owner: ai
  - created_at: 2026-06-13
  - phase: Phase 3 - Generic Slot Layout
  - depends_on: TODO-4668
  - scope: Modify the slot layout resolver to count slots from
    the `.prime` struct's field declarations instead of branching
    on known type names. Handle `Pointer<T>`, `Reference<T>`,
    and `uninitialized<T>` as generic layout primitives.
  - acceptance:
    - `isVectorTypeName()` / `isMapTypeName()` branches removed
      from slot layout code
    - Slot count computed from `.prime` field declarations
    - All backend IR tests pass
  - stop_rule: branches removed and tests pass

- [ ] TODO-4670: Remove collection-specific slot layout helpers
  - owner: ai
  - created_at: 2026-06-13
  - phase: Phase 3 - Generic Slot Layout
  - depends_on: TODO-4669
  - scope: Delete `isVectorTypeName()`, `isMapTypeName()` from
    `IrLowererStructSlotLayoutHelpers.cpp` and verify no other
    files reference them for slot layout purposes.
  - acceptance:
    - Functions deleted
    - No remaining references for slot layout
    - All tests pass
  - stop_rule: functions deleted and tests pass

### Cleanup

- [ ] TODO-4671: Remove isVectorTypeName and isMapTypeName after migration
  - owner: ai
  - created_at: 2026-06-13
  - phase: Cleanup
  - depends_on: TODO-4665, TODO-4666, TODO-4667, TODO-4670
  - scope: After all migrations complete, remove
    `isVectorTypeName()`, `isMapTypeName()`,
    `isKeyValueCollectionTypeName()`,
    `isExperimentalCollectionBackingTypeName()`,
    `isCollectionVectorValue()`, `isKeyValueStorageValue()`,
    `isArrayValue()`, and any other collection-specific helper
    functions that are no longer called.
  - acceptance:
    - Dead functions removed
    - No unused collection helper functions remain
    - Full test suite passes
  - stop_rule: dead code removed and tests pass

- [ ] TODO-4672: Migrate isRemovedKeyValueCompatibilityHelper to manifest
  - owner: ai
  - created_at: 2026-06-13
  - phase: Phase 1 - Surface Manifest Extension
  - depends_on: TODO-4657
  - scope: Replace the hardcoded method name set at
    `SemanticsValidatorExprMethodTargetResolution.cpp` lines 24-32
    (`isRemovedKeyValueCompatibilityHelper`: "count", "count_ref",
    "size", "contains", "contains_ref", "tryAt", "tryAt_ref", "at",
    "at_ref", "at_unsafe", "at_unsafe_ref", "insert", "insert_ref")
    with a `StdlibSurfaceRegistry` query.
  - acceptance:
    - Hardcoded set at lines 24-32 removed
    - Key-value compatibility helper check uses registry query
    - Semantics tests pass
  - stop_rule: set removed and tests pass

- [ ] TODO-4673: Migrate method dispatch chains in MethodTargetResolution
  - owner: ai
  - created_at: 2026-06-13
  - phase: Phase 1 - Surface Manifest Extension
  - depends_on: TODO-4658, TODO-4672
  - scope: Replace the hardcoded method name chains at
    `SemanticsValidatorExprMethodTargetResolution.cpp` line 130
    (`isReadOnlyCollectionMemberHelperName`) and lines 3328-3331
    (method name matching) with manifest-driven queries.
  - acceptance:
    - Hardcoded chains at lines 130 and 3328-3331 removed
    - Method dispatch uses registry lookup
    - Semantics tests pass
  - stop_rule: chains removed and tests pass

- [ ] TODO-4674: Migrate SOA helper routing beyond to_aos
  - owner: ai
  - created_at: 2026-06-13
  - phase: Phase 1 - Surface Manifest Extension
  - depends_on: TODO-4661
  - scope: Migrate remaining SOA helper routing in
    `SemanticsBuiltinPathHelpers.cpp` (lines 931-978: hardcoded
    `soaVectorCount`, `soaVectorCountRef`, `soaVectorGet`,
    `soaVectorGetRef`, `soaVectorRef`, `soaVectorRefRef`,
    `soaVectorPush`, `soaVectorReserve` path comparisons) and
    `SemanticsValidate.cpp` (lines 3835-3895: `soaVectorNew`,
    `soaVectorSingle`, `soaVectorFromAos` path construction).
  - acceptance:
    - Hardcoded SOA helper names in `SemanticsBuiltinPathHelpers.cpp`
      lines 931-978 replaced with manifest lookup
    - At least the `soaVectorNew`/`soaVectorSingle` routing in
      `SemanticsValidate.cpp` lines 3835-3895 replaced
    - SOA tests pass
  - stop_rule: two files migrated and tests pass

- [ ] TODO-4675: Migrate ContainerError hardcoded paths to manifest
  - owner: ai
  - created_at: 2026-06-13
  - phase: Phase 1 - Surface Manifest Extension
  - depends_on: TODO-4657
  - scope: Replace hardcoded `/std/collections/ContainerError/why`
    and `/std/collections/ContainerError/status` path lookups in
    `IrLowererResultHelpers.cpp` (lines 71-73, 1414, 1456) and
    `IrLowererPackedResultHelpers.cpp` (lines 148-150) with
    manifest-driven resolution via the existing
    `CollectionsContainerErrorHelpers` registry entry.
  - acceptance:
    - `IrLowererResultHelpers.cpp` lines 71-73 use manifest lookup
    - `IrLowererPackedResultHelpers.cpp` lines 148-150 use manifest
      lookup
    - Backend IR tests pass
  - stop_rule: two files migrated and tests pass
