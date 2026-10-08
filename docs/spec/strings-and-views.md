# Strings, Text and Slices

> Part of the [PrimeStruct specification index](../PrimeStruct.md). Classification: **design direction**.

Status: designed 2026-10-08, not implemented. The implementation slices are TODO-5537 to TODO-5542 (and TODO-5530 for
whole-file text); until they land, the current behavior is the one in [VM Design](vm-design.md) and
[Backend Type Support](backend-type-support.md). This part refines the view model of
[Pointers and References](pointers-and-references.md) and does not replace it.

## Overview

Text and contiguous data use three types, all on every backend (VM, native, C++):

| Type | What it is | Owns its memory | Mutable |
| --- | --- | --- | --- |
| `Slice<T, Capability>` | a borrowed range of `T` values in contiguous storage | no | with `Write` |
| `string` | a borrowed, read-only view of valid UTF-8 text | no (literals are static) | no |
| `String` | owned UTF-8 text, the `std::string` of PrimeStruct | yes | yes |

- `Slice` is the multi-element form of the existing `View<T, Capability>` model (`Reference<T, Capability>` is the
  single-element form). Every contiguous container hands out slices, and the operations that only need "a range of
  elements" are written once, on `Slice`.
- `string` is to text what `Slice<u8, Read>` is to bytes, with one extra guarantee: its bytes are valid UTF-8. It is
  the type of string literals and the parameter type for "any text".
- `String` owns its bytes, grows, and is freed by the lifecycle rules of [Value Lifecycle](value-lifecycle.md). It
  converts to `string` wherever text is expected.

The split is the one between Rust's `str` and `String`. Keeping the name `string` for the view means existing code
that takes `[string]` parameters, returns literals or prints literals keeps its meaning.

## Bytes

- **`u8`** is a scalar type: an unsigned 8-bit integer. Literals are written `65u8`; `convert<u8>(x)` keeps the low 8
  bits of an integer. Arithmetic on two `u8` values wraps mod 256; mixing `u8` with a wider integer requires
  `convert`. `u8` is `Comparable`, `Additive` and `Multiplicative`.
- **Byte memory.** Containers of `u8` (`array<u8>`, `Vector<u8>`, `Buffer<u8>`) and text store one byte per element,
  packed. On native and C++ this is ordinary byte-addressed memory. The VM's heap gains byte regions: an allocation made
  for bytes is addressed byte by byte (address + i is byte i), while slot-sized values keep their 16-byte slots. IR
  gains `LoadU8`/`StoreU8` (byte-sized load and store at any byte address of a byte region) and `HeapAllocBytes`; a
  slot load from a byte region, or a byte load from a slot region, is a VM fault.

## Slices

### Making a slice

```prime
[Vector<i32> mut] values{vector<i32>(4i32, 7i32, 9i32, 11i32)}
[Slice<i32, Read>] all{values.view()}
[Slice<i32, Read>] middle{values.slice(1i32, 3i32)}   // 7, 9
[Slice<i32, Write>] tail{values.sliceMut(2i32, 4i32)}
```

- `view()` covers the whole container; `slice(start, end)` and `sliceMut(start, end)` cover `[start, end)`. The range
  is checked once when the slice is made (`0 <= start <= end <= count`); a statically known bad range is a compile
  error, a run-time one a fault. `slice(values, start, end)` stays as the free-function spelling.
- A slice of a slice narrows it and borrows the same owner.
- Contiguous containers: `array<T>`, `Vector<T>`, `Buffer<T>`, SoA columns, `String` (as text, see below) and its
  bytes. `map` and `soa_vector` rows are not contiguous and have no `view()`.
- A slice is a pointer and a count. It never copies elements; `toOwned()` copies them into a new `Vector<T>`.
- `Slice<T, Capability>` no longer lowers to a copied `array<T>`; the `slice(...)` of an `array` returns a real
  slice.

### Borrowing

A slice is a loan on its owner's root binding, under the rules of `docs/SafeArrayExtentViews.md` (Borrow Checking For
Views), checked lexically until the slice's last use:

- Read slices may overlap each other. A `Write` slice conflicts with any other live slice or reference into the same
  owner (the first version treats any two ranges of one owner as overlapping).
- While a slice is live, its owner cannot be structurally changed (push, pop, insert, erase, reserve, clear, assign),
  moved, passed to a `mut` or `move` parameter, or destroyed. The diagnostic is `borrowed binding: <root>`.
- A slice cannot outlive its owner and cannot be stored: not in a struct field, a container, an outer binding, or a
  global. The diagnostic is `slice escapes via <place> (root: <root>)`.
- **Returning a slice.** A definition may return a slice (or `string`) that is derived from exactly one of its
  slice, `string`, reference or container parameters. The caller treats the result as a loan on whatever it passed for
  that parameter. A definition whose returned view could come from more than one parameter must say which with
  `[returns_borrow<name>]`; without it, the return is rejected.

### Operations on every slice

Written once in `/std/collections/slice` and available on every `Slice<T, Capability>`:

| Group | Operations |
| --- | --- |
| Size and access | `count()`, `empty()`, `[i]`, `first()`, `last()` (`Maybe<T>`), `at(i)` (`Result<T, ContainerError>`) |
| Narrowing | `slice(start, end)`, `takeFirst(n)`, `dropFirst(n)`, `takeLast(n)`, `dropLast(n)`, `splitAt(i)` |
| Search (`T` `Comparable`) | `find(value)`, `rfind(value)`, `find(other: Slice<T>)`, `contains(...)`, `startsWith(...)`, `endsWith(...)`, `count(value)` |
| Splitting | `split(separator)` yields the pieces between separators as slices of the same owner |
| Comparison | `==` (element-wise), `<` and friends (lexicographic, `T` `Comparable`) |
| Iteration | index loops (`for([i32 mut] i{0i32}, i < s.count(), i++) { ... }`); a range-`for` form is a separate language change |
| Writes (`Write` only) | `[i] = v`, `fill(v)`, `copyFrom(other)` (same count), `reverse()`, `sort()` (`T` `Comparable`), `swap(i, j)` |
| Copying out | `toOwned() -> Vector<T>` |

Generic slice code is instantiated per element size, not per container: a `Slice<u8, Read>` made from a `String`,
a `Vector<u8>` or an `array<u8>` runs the same lowered loops, which keeps bytecode small (see TODO-5536).

## `string`: the text view

- A `string` is a read-only view of valid UTF-8 bytes. It is a slice in every respect above (borrowing, escape rules,
  returning from a parameter) except that it can only be made in ways that keep the text valid.
- **Literals** are static `string`s: they point at constant data, never allocate, and may be stored anywhere,
  including struct fields. A struct field of type `string` may only hold static text; assigning a borrowed `string`
  to one is rejected (`string field requires static text; use String`).
- **Making one:** a literal; `text.view()` or an implicit borrow of a `String` where a `string` is expected;
  `text.slice(start, end)` on a `String` or `string`; `utf8(bytes)` on a `Slice<u8, Read>`, which checks the bytes and
  returns `Result<string, TextError>`.
- **Indices are byte offsets**, as in `std::string`. `count()` is the byte count; `text[i]` is the byte `u8` at `i`.
  Slicing at an offset that is not a code-point boundary is an error (a compile error when known, otherwise a fault),
  so a `string` is always valid UTF-8.
- **Operations:** all read-only slice operations with `string` or `u8` arguments (searching for a `string` finds a
  substring), plus:

| Group | Operations |
| --- | --- |
| Bytes and code points | `bytes() -> Slice<u8, Read>`, `codePoints()` (iterate `u32` code points with their byte offsets), `codePointCount()` |
| Lines and words | `lines()`, `split(string)`, `trim()`, `trimStart()`, `trimEnd()` |
| Case (ASCII) | `toAsciiLower() -> String`, `toAsciiUpper() -> String`, `equalsIgnoreAsciiCase(string)` |
| Parsing | `parseI32()`, `parseI64()`, `parseF64()`, each `Result<T, TextError>` |
| Building | `+` with another `string` or `String` returns a new `String`; `repeat(n) -> String`; `toOwned() -> String` |

- Comparison is byte-wise (`==`, `<`), which orders UTF-8 text by code point. Printing takes a `string`.

## `String`: owned text

`String` is a value type that owns its bytes. A copy copies the text; a move transfers it; destruction frees it.
Every operation keeps the text valid UTF-8.

| `std::string` | `String` |
| --- | --- |
| `size()`, `empty()`, `capacity()` | `count()`, `empty()`, `capacity()` |
| `reserve(n)`, `shrink_to_fit()`, `clear()` | `reserve(n)`, `shrinkToFit()`, `clear()` |
| `append`, `+=`, `push_back` | `append(string)`, `+=`, `push(u32 codePoint)` |
| `operator+` | `+` (returns a new `String`) |
| `insert(pos, s)`, `erase(pos, n)`, `replace(pos, n, s)` | `insert(at, string)`, `erase(start, end)`, `replace(start, end, string)` |
| `substr(pos, n)` | `substr(start, end) -> String` (a copy); `slice(start, end) -> string` (a borrow) |
| `find`, `rfind`, `starts_with`, `ends_with`, `compare` | the `string` operations, through an implicit borrow |
| `c_str()`, `data()` | always NUL-terminated; host calls get the bytes without a copy |
| `std::to_string(x)` | `String.from(x)` for integers, floats and `bool` |
| `std::string(s)` | `String.from(s)`, or initializing a `String` from a `string` |

- Making one: `String{}` (empty; braces map to fields as for any struct, so text is never passed this way),
  `String.from(text)` (copies a `string`), `String.from(x)` for numbers and `bool`, `String.withCapacity(n)`,
  `String.fromUtf8(bytes)` (`Result<String, TextError>`), and `text.toOwned()`. Initializing a `String` binding,
  field or parameter from a `string` copies the text: `[String] name{"draft"}`.
- While a `string` borrowed from a `String` is live, the `String` cannot be changed or moved (the slice rules above).

### Memory

`String` memory is predictable and the same on every backend.

- **Inline:** text up to 22 bytes is stored inside the `String` value and allocates nothing.
- **String heap:** longer text comes from a heap of 256 KiB chunks divided into size classes, four per doubling from 32
  bytes to 64 KiB, so at most about 25% of a block is unused. A freed block goes back on its class's free list and is
  reused. A chunk whose blocks are all free is returned to the system, except one kept for reuse. The first chunk is
  made on the first long string, so programs that never need one pay nothing.
- **Large text:** text over 64 KiB gets its own allocation, returned to the system as soon as it is freed.
- **Growth:** capacity doubles, rounded up to the size class; growing within a class does not move the text.
  `reserve(n)` gives exactly the class that holds `n`; `shrinkToFit()` moves the text to the smallest class that holds
  it.
- **Observability:** `stringMemory()` returns `StringMemoryStats { chunks, bytesInUse, bytesFree, largeBlocks,
  largeBytes }`. `setStringMemoryLimit(bytes)` caps the heap; an allocation past the cap is the runtime fault
  `string memory limit exceeded`, and `String.tryReserve(n)` returns `Result<void, TextError>` instead of faulting.
- **Threads:** each thread has its own string heap; a `String` freed on another thread returns its block to the heap
  it came from.
- **Later:** scoped arenas for batch work (`withStringArena { ... }`), whose `String`s are freed together and cannot
  escape the block.

## Host functions and embedding

- A `[host]` parameter may be `string` or `String`; the host receives a UTF-8 pointer and length. A `String` (always
  NUL-terminated) and a static literal pass without a copy; another `string` passes as a temporary NUL-terminated copy.
- A host function that returns text returns a `String`. The embedding API converts `std::string` results into the
  program's string heap, and the program frees them like any other `String`.
- The VM's current run-time strings (a `u64` with bit 63 set, created only by host calls and never freed during a run)
  are replaced by `String`; the PSIR format version changes when that lands.

## Migration

- Code that takes `[string]`, prints literals or returns literals is unchanged.
- A binding, field or return that holds text created at run time (host results, built text) becomes `String`. The
  compiler reports `run-time text requires String` where a `string` would need to own its text.
- `Slice<T, Capability>` parameters that today receive a copied `array<T>` receive a real slice; code that relied on
  the copy being independent of its source keeps working because the source cannot change while the slice is live.

## Implementation slices

| TODO | Slice |
| --- | --- |
| TODO-5537 | `u8` and byte-addressed memory (VM byte regions, `LoadU8`/`StoreU8`) |
| TODO-5538 | `slice(...)` as a real borrow, the return-from-parameter rule, and the shared `Slice<T>` operations on `array` and `Vector` |
| TODO-5539 | `String`: value, inline storage, lifecycle, the string heap and the owned-text API |
| TODO-5540 | `string` as the UTF-8 view: static literals, conversions, text operations, field rule |
| TODO-5530 | Read and write whole files as `String` |
| TODO-5541 | Host functions and the embedding API use `String`/`string`; retire VM run-time string indices |
| TODO-5542 | Native and C++ parity for `u8`, slices, `string` and `String` |
