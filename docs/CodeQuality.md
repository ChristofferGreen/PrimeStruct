# PrimeStruct Code Quality

Status: Draft
Last updated: 2026-04-17

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
clampToZero([int] value) {
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
runCountdown([int] start) {
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

### Simple Struct Helper

Keep struct helpers small and make their names match their behavior.

```prime
[struct]
Counter {
  [int] value{0}

  [int]
  nextValue() {
    return(this.value + 1)
  }
}
```

Why this is good:
- The struct has one clear piece of state.
- The helper name does not imply mutation.
- Data and related behavior stay together.

### Vector Loop with Current Verified Surface

For vector examples, keep the currently verified import and constructor spelling
visible.

```prime
import /std/collections/*

[effects(heap_alloc), int]
sumValues() {
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

Current note:
- Use `import /std/collections/*` for bare vector helper names and method-sugar
  examples.
- Exact `import /std/collections/vector` is now a supported current style for
  bare `vector(...)` construction plus canonical `/std/collections/vector/*`
  helper surface.
- Concise bindings such as `[vector<int>] values{4, 8, 15}` are now part of the
  verified current style for examples that use wildcard collection imports.
- The explicit constructor form `vector<T>{...}` remains valid, but docs no
  longer need it as a fallback for the readable vector loop example.

## Example Hygiene

When adding or editing examples in docs:
- Prefer top-level surface spelling unless the example is explicitly about
  canonical form.
- Keep examples minimal but runnable.
- Re-check examples with the current compiler before treating them as style
  guidance.
