# Strings, Text and Slices

> Part of the [PrimeStruct specification index](../PrimeStruct.md). Classification: **design direction**.

Status: designed 2026-10-08. The bytes, `u8`, packed `vector<u8>` and the owned `String` with its string heap are
implemented on the VM (TODO-5537, TODO-5565, TODO-5566, TODO-5539); slices, the `string` view, host strings, whole-file
text and native/simple-C++ parity are still open (TODO-5540, TODO-5618 to TODO-5622, TODO-5530, TODO-5574). Until those land, the current behavior is the one in [VM Design](vm-design.md) and
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

### Current surface (VM)

`Slice<T, Capability>` is implemented as a stdlib struct pair in `stdlib/std/collections/slice.prime`
(generated by `scripts/generate_slice_stdlib.py`; `import /std/collections/slice/*`). The type resolver maps
`Slice<T, Read>` to `ReadSlice<T>` and `Slice<T, Write>` (or `ReadWrite`) to `WriteSlice<T>`; a read view has
no write operations and `WriteSlice<T>.readOnly()` gives a read view of the same elements. Views of a
`vector<T>` come from `/std/collections/vector/view<T>`, `slice<T>` and `sliceMut<T>`; a view narrows with
`slice(start, end)` and keeps its capability. The operations are `count`, `empty`, `at`, `first`, `last`
(`Maybe<T>`), `find`, `rfind`, `contains`, `occurrences` (spelled `count(value)` in the table), `equals`,
`compare`, `startsWith`, `endsWith`, `findSlice`, `takeFirst`, `dropFirst`, `takeLast`, `dropLast`, `head`,
`tail`, `splitPart(separator, n)`, `splitCount(separator)` and, on a write view, `set`, `fill`, `swap`,
`reverse`, `copyFrom`, `sort`. Not yet: the `values.view()`/`slice()`/`sliceMut()` method spellings, `[i]` on
a view, views of `array<T>` and the loan rules (docs/todo.md).

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

- **No inline storage.** An empty `String` has no storage and allocates nothing; any text, even one byte, takes a
  block from the string heap (the smallest is 32 bytes, 31 for text). The first design stored up to 22 bytes inside the
  value, but a VM value is a run of 16-byte slots, and bytes inside slots cannot be addressed or borrowed as text, so
  inline text would break `string` views and `Slice<u8>` on the VM.
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

### Current surface (VM)

Until the `string` view and slices land (TODO-5540, TODO-5538), `String` is a stdlib struct in
`stdlib/std/text/string.prime` (`import /std/text/*`) over the string heap, whose block addresses are byte-region
addresses of the VM heap. The VM answers the `__pstext_*` host functions itself (they need no binding); `Pointer<u8>`
is a valid `[host]` parameter and result type and crosses as the block's `u64` address.

- Making one: `String{}`, `/std/text/stringFrom(text)`, `/std/text/stringWithCapacity(n)`. A binding or argument
  copies through the type's `Copy` helper; a `[String copy]` parameter takes its own copy.
- Methods: `count()`, `empty()`, `capacity()`, `byteAt(i)` (a fault outside the text), `equalsText(text)`,
  `terminator()` (always 0), `reserve(n)`, `tryReserve(n) -> bool`, `shrinkToFit()`, `clear()`, `pushByte(u8)`,
  `append(text)`, `appendString(other)`, `insert(at, text)`, `erase(start, end)`, `replace(start, end, text)`,
  `substr(start, end) -> String`.
- Heap: `/std/text/stringHeapChunks()`, `stringHeapBytesInUse()`, `stringHeapBytesFree()`,
  `stringHeapLargeBlocks()`, `stringHeapLargeBytes()` and `setStringMemoryLimit(bytes)`. `text` parameters are the
  current literal-backed `string`, read byte by byte; they become real views with TODO-5540.
- Not yet: `+` and `+=`, `push(u32)`, `String.from(number)`, `fromUtf8`, the `Result`-returning `tryReserve`, per-thread
  heaps and arenas (TODO-5568).

## Parity with `std::string` and `std::string_view`

Quality bar: `String` and `string` are no less capable than `std::string` and `std::string_view`, with UTF-8 text
operations on top. Every C++ member has a PrimeStruct spelling or a stated difference. "Done" means on the VM today;
the other entries are specified here and tracked by the TODO named.

| C++ | PrimeStruct | Status |
| --- | --- | --- |
| `string()`, `string(const char*)`, `string(sv)` | `String{}`, `String.from(text)`, `[String] s{"text"}` | done as `stringFrom`; spelling TODO-5568 |
| `string(n, ch)`, `assign(n, ch)` | `String.filled(n, byte)`, `fill(n, byte)` | TODO-5623 |
| `string(sv, pos, n)`, `substr` | `text.slice(a, b).toOwned()`, `substr(a, b)` | `substr` done; `slice` TODO-5618 |
| construct from raw bytes | `String.fromUtf8(bytes)`, `String.fromUtf8Lossy(bytes)` | TODO-5568, TODO-5623 |
| copy / move, destructor | `Copy`, `Move`, `Destroy` lifecycle | done |
| `at`, `operator[]` | `byteAt(i)`, `[i]`, `at(i)` | `byteAt` done; operators TODO-5568 |
| `front`, `back` | `first()`, `last()` | TODO-5623 |
| `data`, `c_str` | the block, always NUL terminated | done |
| `operator string_view` | implicit borrow as `string` | TODO-5618 |
| `begin`/`end` iterators | `bytes()`, `codePoints()`, index loops (range-`for` is a separate language change) | TODO-5618, TODO-5620 |
| `empty`, `size`, `capacity`, `reserve`, `shrink_to_fit`, `clear` | `empty`, `count`, `capacity`, `reserve`, `shrinkToFit`, `clear` | done |
| `resize` | `resize(n, byte)` | TODO-5623 |
| `push_back`, `pop_back` | `pushByte`, `push(codePoint)`, `popByte`, `pop()` | `pushByte` done; rest TODO-5568, TODO-5623 |
| `append(sv)`, `append(n, ch)`, `operator+=` | `append`, `appendFilled`, `+=` | `append` done; rest TODO-5568, TODO-5623 |
| `insert`, `erase`, `replace` | `insert`, `erase`, `replace` (byte ranges) | done; byte and fill overloads TODO-5623 |
| `operator+` | `+` | TODO-5568 |
| `swap` | `swap(other)` | TODO-5623 |
| `copy(dst, n, pos)` | `slice(a, b).copyTo(destination)` | TODO-5623 |
| `compare`, `==`, `<=>` | `compare(other)`, `==`, `<` and friends (byte order) | TODO-5619 |
| `starts_with`, `ends_with`, `contains` | `startsWith`, `endsWith`, `contains` | TODO-5619 |
| `find`, `rfind` (with `pos`) | `find(text, from)`, `rfind(text, from)` | TODO-5619 |
| `find_first_of`, `find_last_of`, `find_first_not_of`, `find_last_not_of` | same names in lowerCamelCase over a byte set | TODO-5624 |
| `to_string`, `stoi`, `stol`, `stod`, `from_chars` | `String.from(x)`, `parseI32/I64/U64/F64` | TODO-5568, TODO-5620 |
| `to_chars` (radix, precision, shortest round trip) | `String.from(x, radix)`, `formatF64(x, precision)`, `String.from(f64)` shortest | TODO-5626 |
| `std::hash<string>`, `unordered_map<string, V>`, `map<string, V>` | `String` and `string` as `map` keys, `hash()` | TODO-5625 |
| `std::format`, `operator<<`, `getline` | `format("{}", ...)`, `join`, `lines()`, `readText` | TODO-5627, TODO-5619, TODO-5530 |
| `memcpy`/`memmove`/`memcmp`/`memchr` speed | bulk byte opcodes under every copy, compare and search | TODO-5629 |
| `string_view::remove_prefix`/`remove_suffix`/`substr` | `dropFirst`, `dropLast`, `slice` | done on `Slice`; `string` TODO-5618 |

Beyond C++: UTF-8 validity, code points, `trim`, `split`, `lines`, `join`, ASCII case, `repeat`, `replaceAll`,
`Result`-typed parsing.

Intended differences, recorded so they stay deliberate:

- **Text is valid UTF-8.** `std::string` holds any bytes; `String` does not. Arbitrary bytes are `Vector<u8>` or
  `Slice<u8>`, with `String.fromUtf8`/`fromUtf8Lossy` and `bytes()` converting either way without surprises.
- **No inline small-string storage** (see Memory above). TODO-5628 measures short-string workloads against
  `std::string` and decides from the numbers whether a small-string form is worth its cost on the VM.
- **Counts and offsets are `i32` today**, so text is capped at 1 GiB (`reserve` rejects more). Whether `count()` moves
  to `u64` follows the language's integer-width decisions; TODO-5628 records the cap as a known limit of the
  conformance suite.

## Beyond `std::string`

Where the language's own rules let text be better than the C++ type, and the TODO that delivers it:

| Idea | What it gives | TODO |
| --- | --- | --- |
| Borrow-checked views | no dangling `string_view`; no iterator invalidation | TODO-5569, TODO-5618 |
| Structs that hold views | a parsed `Request` of views into its buffer: zero-copy and safe | TODO-5631 |
| Request-scoped string arenas | all of a request's text freed in O(1) | TODO-5630 |
| Fused `+` chains and one-allocation `format` | one block per expression, guaranteed | TODO-5632, TODO-5627 |
| `Maybe`/`Result` APIs | no `npos`, no exceptions, no undefined behavior | TODO-5619, TODO-5620 |
| Linear-time search | `find` never goes quadratic | TODO-5619 |
| Byte-identical text on every backend | locale-free number and case output | TODO-5626 |
| Keyed hash, `Symbol` interning | flood-resistant keys; O(1) equality for names | TODO-5625, TODO-5636 |
| UTF-8 invariant, Unicode case, normalization, graphemes | correct text, loaded only when used | TODO-5633, TODO-5634 |
| `Secret<String>` | zeroed on destroy, never printed, constant-time compare | TODO-5635 |
| Compile-time folding of literal operations | no run-time work for constants | TODO-5637 |
| `SharedString` | cheap immutable sharing across threads (deferred with concurrency) | TODO-5638 |

Typed, context-escaped text (`Html`, `Url`, ...) was considered and left out for now.

### Measured against `std::string`

`tests/unit/backend_host/test_string_conformance.cpp` (suite `primestruct.backend.string_conformance`) runs seeded
edit sequences (append, push, insert, erase, replace, substr, reserve, shrink) and sizes around the heap block
classes (0 to 70000 bytes) on the VM and through `--emit=optexe`, and compares the printed checksums with a
`std::string` oracle; they agree. `python3 scripts/benchmark_strings.py --build-dir build-release --budget
benchmarks/strings/budget.json` times `optexe` against `std::string` on the six workloads of `benchmarks/strings/`.
Medians of repeated runs on macOS arm64 (2026-11-02, release build):

| Workload | `std::string` | `String` (`optexe`) | Ratio | Budget |
| --- | --- | --- | --- | --- |
| `append_bytes`: 5M `pushByte` | 0.03 s | 0.10 s | 3x | 5 |
| `append_chunks`: 200k appends of 43 bytes | 0.02 s | 0.07 s | 2 to 3x | 5 |
| `short_strings`: 1M `"GET"` + 12 bytes | 0.02 s | 0.17 s | 9x | 15 |
| `insert_front`: 20k inserts at 0 | 0.02 s | 0.02 s | 1.0 to 1.3x | 2.5 |
| `copy_text`: 200k copies of 1000 bytes | 0.03 s | 0.04 to 0.05 s | 1.3 to 1.7x | 3 |
| `replace_middle`: 100k replaces | 0.02 s | 0.03 s | 1.1 to 1.3x | 2.5 |

Two steps got there. The string heap fixes of TODO-5640 (an O(1) double-free check, a per-class index of chunks with
free blocks, an empty-chunk counter and a one-entry region cache in `byteAt`) took `short_strings` from 110 to 140x
down to about 9x. The bulk byte host functions of TODO-5629 (`__pstext_move`, `_fill`, `_compare`, `_find`, one
range check per call, then a library loop on the VM and `optexe` and a plain byte loop on the arm64 native backend)
took `insert_front`, `copy_text` and `replace_middle` from 20 to 60x to about 1 to 2x. What is left: appending a
string literal still copies byte by byte from the module string table until the string view lands (TODO-5540), and
every non-empty `String` takes a heap block (`short_strings`). Lower a budget whenever its ratio drops.

Found by the suite: a binding initialized from another `String` binding aliases the heap block and frees it twice
(TODO-5639); the supported way to duplicate text is a `[String copy]` parameter. Known limits: counts are `i32`, so text
is capped at 1 GiB (`reserve` rejects more).

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
| TODO-5537 (done) | IR byte opcodes (`LoadU8`/`StoreU8`/`HeapAllocBytes`/`ZextU8`) and VM byte regions |
| TODO-5565 (done) | the `u8` scalar type |
| TODO-5566 (done), TODO-5567 | packed byte containers: `vector<u8>` and `Pointer<u8>` buffers, then `array<u8>` |
| TODO-5538 (done), TODO-5569, TODO-5570 | `Slice<T, Capability>` as a real view with shared operations; loan and escape rules; method spellings and `array<T>` views |
| TODO-5539 (done) | `String`: value, lifecycle, the string heap and the owned-text API |
| TODO-5540 | `string` as a static view: literals, count, bytes, equality |
| TODO-5618 | `string` borrowed from `String`, `view()`, `slice()`, `utf8()` |
| TODO-5619 | `string` search, ordered comparison, trim, `split`, `lines` |
| TODO-5620 | `string` parsing, code points, ASCII case |
| TODO-5621 | static-text field rule and `run-time text requires String` |
| TODO-5622 | optimized C++ emitters (`optcpp`, `optexe`) run the view |
| TODO-5630 to TODO-5638 | beyond `std::string`: arenas, view-holding structs, fused concatenation, Unicode, `Secret`, `Symbol`, literal folding, `SharedString` (see Beyond above) |
| TODO-5623 to TODO-5629 | `std::string` parity: mutation, search sets, hashing and map keys, number formatting, `format`/`join`, conformance and speed gate, bulk byte opcodes (see Parity above) |
| TODO-5530 | Read and write whole files as `String` |
| TODO-5541 | Host functions and the embedding API use `String`/`string`; retire VM run-time string indices |
| TODO-5574 | Simple C++ emitters and native parity for `u8`, slices, `string` and `String` (the optimized C++ emitters are done) |
