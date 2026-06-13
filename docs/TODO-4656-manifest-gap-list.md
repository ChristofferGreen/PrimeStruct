# TODO-4656: Surface Manifest Coverage Gap List

Generated: 2026-06-13

## Manifest Members (surfaces.psmeta)

| Surface | Members |
|---------|---------|
| CollectionsVectorHelpers | vector, count, capacity, push, pop, reserve, clear, remove_at, remove_swap, at, at_unsafe |
| CollectionsMapHelpers | entry, map, count, count_ref, contains, contains_ref, tryAt, tryAt_ref, at, at_ref, at_unsafe, at_unsafe_ref, insert, insert_ref |
| CollectionsColumnarHelpers | count, count_ref, get, get_ref, ref, ref_ref, field_view, reserve, push, to_aos |
| CollectionsColumnarConstructors | soa, single, from_aos |
| CollectionsVectorConstructors | vector |
| CollectionsMapConstructors | map |

## Hardcoded Names in C++ (by file)

### File 1: `SemanticsValidatorExprMethodTargetResolution.cpp`

**Lines 17-22 — `isRemovedVectorCompatibilityHelper`:**
count, capacity, at, at_unsafe, push, pop, reserve, clear, remove_at, remove_swap

**Lines 24-32 — `isRemovedKeyValueCompatibilityHelper`:**
count, count_ref, size, contains, contains_ref, tryAt, tryAt_ref, at, at_ref, at_unsafe, at_unsafe_ref, insert, insert_ref

### File 2: `IrLowererBuiltinNameHelpers.cpp`

**Lines 86-126 — `isNamespacedStdlibBuiltinAlias`:**

Collection-specific:
- count, count_ref, capacity, to_aos, to_aos_ref, push, pop, reserve, clear, remove_at, remove_swap, move, get, get_ref, ref, ref_ref, at, at_unsafe, array, vector

Language/math builtins (NOT collection-specific):
- assign, if, while, take, borrow, init, drop, increment, decrement, return, then, else, do, block, loop, for, repeat, try, location, dereference, negate, plus, minus, multiply, divide, greater_than, less_than, equal, not_equal, greater_equal, less_equal, and, or, not, convert, clamp, min, max, lerp, fma, hypot, copysign, radians, degrees, sin, cos, tan, atan2, asin, acos, atan, sinh, cosh, tanh, asinh, acosh, atanh, exp, exp2, log, log2, log10, abs, sign, saturate, pow, is_nan, is_inf, is_finite, floor, ceil, round, trunc, fract, sqrt, cbrt

### File 3: `EmitterBuiltinCallPathHelpers.cpp`

**Lines 44-84 — `isNamespacedStdlibBuiltinAlias`:**

Same as File 2, plus:
- `soa_vector` (line 69) — legacy alias token not in File 2

## Gap Analysis

### Collection-specific names in C++ but NOT in manifest

| Name | File(s) | Category | Manifest Status |
|------|---------|----------|-----------------|
| `size` | MethodTargetResolution:26 | Alias for `count` in key-value context | **MISSING** — should be `compatibility_spelling` or `member_alias` for `count` |
| `to_aos_ref` | IrLowerer:97 | Borrowed receiver variant of `to_aos` | **MISSING** — should be `borrowed_variant` of `to_aos` |
| `soa_vector` | Emitter:69 | Legacy alias token | **MISSING** — should be `import_alias_spelling` or `compatibility_spelling` |
| `move` | IrLowerer:100 | Generic lifecycle helper | **NOT A COLLECTION MEMBER** — language builtin |
| `array` | IrLowerer:109 | Type constructor | **NOT A COLLECTION MEMBER** — language builtin |
| `vector` | IrLowerer:110, Emitter:68 | Type constructor | Already in manifest as `import_alias_spelling` |

### Collection-specific names in BOTH C++ and manifest (already covered)

All members of CollectionsVectorHelpers, CollectionsMapHelpers, and CollectionsColumnarHelpers are present in both the manifest and the hardcoded C++ code. No gaps here.

### Non-collection names (NOT candidates for manifest)

The 70+ language/math builtin names (`assign`, `if`, `while`, `plus`, `minus`, `sin`, `cos`, etc.) are NOT collection-specific. They are language-level builtins that should remain in the `isNamespacedStdlibBuiltinAlias` function. These are NOT gaps.

## Summary

**3 gaps found:**

1. **`size`** (MethodTargetResolution:26) — key-value compatibility alias for `count`. Add as `compatibility_spelling` to CollectionsMapHelpers.
2. **`to_aos_ref`** (IrLowerer:97) — borrowed variant of `to_aos`. Add as `borrowed_variant` to CollectionsColumnarHelpers.
3. **`soa_vector`** (Emitter:69) — legacy alias token. Add as `compatibility_spelling` or `import_alias_spelling` to CollectionsColumnarHelpers or CollectionsColumnarConstructors.

**Actionable for TODO-4657:** The `to_aos_ref` gap directly feeds into the borrowed receiver variant metadata work.
