# PrimeStruct Code Examples

Status: Draft
Last updated: 2026-04-20

This document captures practical code-quality guidance for user-facing
PrimeStruct code examples. It focuses on readable top-level source form rather
than bottom-level canonical spelling.

## Surface Style vs Canonical Form

When writing top-level PrimeStruct examples, prefer the readable surface form
that the compiler expands automatically.

At top level:
- A type inside `[]` on a definition is wrapped into `return<...>`.
- Operators are enabled by default, so `a + b` and `x < y` are preferred over
  canonical helper-call spellings.
- Common string and integer literal suffixes are added automatically.
- Common low-risk effects such as `print_line(...)` are added automatically by
  default.

Bottom-level canonical form still exists and is useful for dumps, debugging,
and spec work, but it is usually too noisy for style-guide examples.

## Stdlib Style Boundary

This style guide applies only to the stdlib modules that are meant to read like
user-facing library code. When a directory mixes public wrappers with
bridge/substrate helpers, use the file-level boundary below instead of assuming
the whole directory shares one style target.

Style-aligned surface code:
- `stdlib/std/math/*`
- `stdlib/std/maybe/*`
- `stdlib/std/file/*`
- `stdlib/std/image/*`
- `stdlib/std/ui/*`
- `stdlib/std/collections/vector.prime`
- `stdlib/std/collections/map.prime`
- `stdlib/std/collections/errors.prime`
- `stdlib/std/collections/soa_vector.prime`
- `stdlib/std/collections/soa_vector_conversions.prime`
- `stdlib/std/gfx/gfx.prime`

These modules should converge on the readable surface forms in this document:
prefer method-style calls, operator syntax, concise inferred locals, and
user-facing names whenever the current language surface already supports them.

Internal implementation, bridge, or substrate-oriented code:
- `stdlib/std/bench_non_math/*`
- `stdlib/std/collections/collections.prime`
- `stdlib/std/collections/experimental_vector.prime`
- `stdlib/std/collections/experimental_map.prime`
- `stdlib/std/collections/experimental_soa_vector.prime`
- `stdlib/std/collections/experimental_soa_vector_conversions.prime`
- `stdlib/std/collections/internal_*`
- `stdlib/std/gfx/experimental.prime`

These files may stay helper-heavy or bridge-oriented while they define
substrate, compatibility, migration, or benchmark behavior. Do not use them as
the style reference for new public-facing examples.

Mixed-directory rule:
- `stdlib/std/collections` is intentionally mixed; follow the file-level list
  above.
- `stdlib/std/gfx` is intentionally mixed; treat `gfx.prime` as the
  style-aligned wrapper layer and `experimental.prime` as bridge-oriented code.

## Code Conventions

- Prefer snake_case for free-standing user-defined functions in examples.
- Prefer lowerCamelCase for struct member functions in examples.
- Keep struct and enum type names in PascalCase.
- Keep example names descriptive and behavior-oriented rather than generic.

## General Guidance

- Prefer small functions with one obvious responsibility.
- Prefer names that describe intent, not mechanism.
- Keep mutation local and explicit.
- Keep control flow shallow and easy to scan.
- When a helper name sounds mutating, make sure the body actually mutates; if
  it does not, choose a non-mutating name.
- Prefer examples that compile and run unchanged under the current toolchain.

## Verified Simple Examples

These examples were validated against the current release toolchain in both VM
and native paths.

### Simple Conditional Helper

Use a small helper with one clear rule and an obvious name.

```prime
[int]
clamp_to_zero([int] value) {
  if (value < 0) {
    return(0)
  } else {
    return(value)
  }
}
```

Why this is good:
- The name states the behavior directly.
- The function has one responsibility.
- The control flow is explicit and short.

### Local Mutation with Narrow Scope

Use mutable locals only when needed, and keep the loop body small.

```prime
[int]
run_countdown(start) {
  [int mut] current{start}

  while(current > 0) {
    print_line("tick")
    current = current - 1
  }

  return(current)
}
```

Why this is good:
- Mutation is limited to one local binding.
- The state name (`current`) explains what changes.
- The side effect is obvious at the call site.

### Inferred Local Bindings

When a local initializer already makes the type obvious, prefer omitted local
envelopes and use `[mut]` only on the binding that actually changes.

```prime
[int]
clamp_to_limit([int] start) {
  [mut] current{start}
  limit{5}

  while(current < limit) {
    current = current + 1
  }

  return(current)
}
```

Why this is good:
- The mutable binding is marked directly without repeating the inferred type.
- The fixed local (`limit`) stays concise because the initializer already shows enough.
- The loop still reads clearly even with the shorter local-binding syntax.

### Borrow Checker with Last-Use Flow

When a borrow is only needed for one read, consume it early and let later
mutation happen after that last use.

```prime
[int]
borrow_checker_window() {
  [int mut] value{4}

  [Reference<int>] shared{location(value)}
  before{shared}

  [Reference<int> mut] exclusive{location(value)}
  exclusive = before + 5

  return(value)
}
```

Why this is good:
- The immutable borrow is used once and becomes inactive before the mutable borrow starts.
- The example shows the current `Reference<T>` surface without dropping into pointer-heavy spelling.
- Moving the mutable borrow above `before{shared}` would trigger a `borrow conflict` diagnostic.

### Simple Struct Helper

Keep struct helpers small and make their names match their behavior.

```prime
[struct]
Counter {
  [int] value{0}

  [int]
  next_value() {
    return(this.value + 1)
  }
}
```

Why this is good:
- The struct has one clear piece of state.
- The helper name does not imply mutation.
- Data and related behavior stay together.

### User-Defined Method Calls

When a struct helper reads like behavior owned by the value, call it through the
instance so the relationship stays visible at the call site.

```prime
[struct]
Counter {
  [int] value{0}

  [int]
  nextValue() {
    return(this.value + 1)
  }
}

counter_result() {
  counter{Counter([value] 4)}
  return(counter.nextValue())
}
```

Why this is good:
- The call site makes the value-behavior relationship obvious.
- The member function stays on the documented lowerCamelCase convention.
- The example shows user-defined method-call syntax without extra control-flow noise.

### Concise Local Binding with Labeled Construction

When the initializer already makes the type obvious, prefer the concise local
binding form and keep constructor labels where they help readability.

```prime
[struct]
Pair {
  [int] left{0}
  [int] right{0}
}

[int]
sum_pair() {
  [Pair] pair{[left] 4, [right] 8}
  return(pair.left + pair.right)
}
```

Why this is good:
- The local binding stays short because the initializer already shows the type.
- Labeled field initializers make field intent obvious at the binding site.
- The example introduces a distinct surface syntax without adding extra control-flow noise.

### Struct Defaults with Partial Construction

When a struct already provides sensible field defaults, override only the field
that matters at the call site instead of restating the whole shape.

```prime
[struct]
Rect {
  [int] width{1}
  [int] height{2}
}

area_with_default_height() {
  rect{Rect([width] 4)}
  return(rect.width * rect.height)
}
```

Why this is good:
- The example shows that omitted fields use their declared defaults.
- The call site stays focused on the one field that changes.
- The helper demonstrates a distinct struct-construction rule without extra syntax.

### Labeled Function Arguments

When a call has multiple parameters with related meanings, labeled arguments can
make the call site easier to scan without changing the underlying helper shape.

```prime
[int]
add_pair([int] left, [int] right) {
  return(left + right)
}

[int]
main() {
  return(add_pair([left] 4, [right] 8))
}
```

Why this is good:
- The labels make it obvious which value fills each parameter role.
- The helper body stays simple while the call site stays self-documenting.
- The example shows a supported labeled-call surface without extra syntax noise.

### Array Literals with Direct Indexing

When the data is fixed-size and local to one expression, an array literal can
keep the example compact without introducing extra bindings.

```prime
[int]
first_plus_last() {
  return(array<int>{4, 8, 15}[0] + array<int>{4, 8, 15}[2])
}
```

Why this is good:
- The literal keeps the data close to the use site.
- The example shows direct indexing on a core array surface.
- The helper stays short while still demonstrating a distinct syntax form.

### Vector Loop with Current Verified Surface

For vector examples, keep the currently verified import and constructor spelling
visible.

```prime
import /std/collections/*

[effects(heap_alloc), int]
sum_values() {
  [vector<int> mut] values{4, 8, 15}
  [int mut] total{0}
  [int] count{values.count()}

  for([int mut] index{0}; index < count; ++index) {
    total = total + values[index]
  }

  return(total)
}
```

Why this is good:
- The example is small but still shows a real collection workflow.
- The required heap-allocation effect is visible at the function boundary.
- The loop shape is explicit and easy to scan.

### Collection Method Sugar

When a collection example is really about the container workflow, prefer the
member-style helper surface over canonical free-function spellings.

```prime
import /std/collections/*

[effects(heap_alloc), int]
accumulate_values() {
  [vector<int> mut] values{1, 2}
  values.push(3)
  values.reserve(8)

  return(values.at(0) + values.at(2) + values.count())
}
```

Why this is good:
- The example shows the preferred method-style collection API directly.
- Mutation stays on the collection that owns the state.
- The return expression demonstrates readable `at(...)` and `count()` usage without extra ceremony.

### Result Propagation with `?`

When an entrypoint crosses from typed `Result` values into integer status codes,
prefer `?` plus an explicit `on_error` handler over manual status plumbing or
ad-hoc unwrap patterns.

```prime
import /std/file/*

[Result<int, FileError>]
count_ready([int] value) {
  return(Result.ok(value + 1))
}

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error(err.why())
}

[int effects(io_err) on_error<FileError, log_file_error>]
main() {
  ready{count_ready(5)}
  return(ready?)
}
```

Why this is good:
- The typed fallible helper stays separate from the entrypoint-style integer status boundary.
- `?` keeps the happy path short without hiding that an early return can happen.
- The example uses the current accepted short envelope and handler spelling.

Current note:
- Use `import /std/collections/*` for bare vector helper names and method-sugar
  examples.
- Exact `import /std/collections/vector` is part of the supported surface for
  bare `vector(...)` construction plus canonical
  `/std/collections/vector/*` helper forms.
- Keep wildcard imports for concise vector bindings, bare helper names,
  method-sugar, and direct indexing examples.
- Concise bindings such as `[vector<int>] values{4, 8, 15}` are now part of the
  verified current style for examples that use wildcard collection imports.
- Omitted parameter envelopes on free-standing definitions are part of the
  verified current surface and are treated as implicit `auto` parameters.
- Labeled struct-literal local bindings such as
  `[Pair] pair{[left] 4, [right] 8}` are part of the verified current surface
  and are preferred over constructor-wrapper spellings when the binding already
  names the type.
- The explicit constructor form `vector<T>{...}` remains valid, but docs no
  longer need it as a fallback for the readable vector loop example.

## Example Hygiene

When adding or editing examples in docs:
- Prefer top-level surface spelling unless the example is explicitly about
  canonical form.
- Keep examples minimal but runnable.
- Re-check examples with the current compiler before treating them as style
  guidance.
