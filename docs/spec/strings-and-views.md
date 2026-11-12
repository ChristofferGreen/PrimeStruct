# Strings, Text and Slices

> Part of the [PrimeStruct specification index](../PrimeStruct.md). Classification: **design direction**.

Status: designed 2026-10-08. The bytes, `u8`, packed `vector<u8>` and the owned `String` with its string heap are
implemented on the VM (TODO-5537, TODO-5565, TODO-5566, TODO-5539); slices, the `string` view, host strings, whole-file
text and native parity are still open (TODO-5540, TODO-5618 to TODO-5622, TODO-5530). Until those land, the current behavior is the one in [VM Design](vm-design.md) and
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

The checker (`validateSliceLoans`, run after the validator) is lexical and name based. A binding of type `Slice`,
`ReadSlice`, `WriteSlice` or `StringView` whose initializer is a call on a binding `o` (`o.view()`, `v.slice(a, b)`,
`firstHalf(o)`) is a loan on `o`'s root, through any chain of views. The loan lasts until the last statement of the block
that mentions the view; a statement before that one that pushes, pops, inserts, erases, appends, replaces, clears,
reserves, resizes, sets, assigns or moves the owner (or a field of it) is rejected with `borrowed binding: <root> (root:
<root>, sink: <view>)`. A view assigned to a field (`slice escapes via field <name> (root: <root>)`), pushed or inserted
into a container (`slice escapes via container`) or returned from a definition while it borrows a local
(`slice escapes via return`) is rejected. A definition that returns views of more than one parameter needs
`[returns_borrow<name>]` (`returned view could borrow from more than one parameter`). Also rejected: passing the owner to a `mut` or `move` parameter of a function it calls (found through the call's
resolved callee) before the view's last use, and a `Write` view next to any other live view of the same owner (the
views it was narrowed from excepted; two `Read` views may overlap): `borrowed binding: <root> (root: <root>, sink:
<view>)`. Overlapping copies inside one owner therefore take their two views as call arguments, not as two
bindings. A call to a definition with `[returns_borrow<name>]` lends only the argument for that parameter. Not
checked: a view stored in a global (the language has none).

A struct with a view field (a *carrier*, directly or through another carrier) is itself a loan: `[Request] req{parseRequest(buffer.view())}`
borrows `buffer` until the last use of `req`, and a constructor such as `Request{[method] line.slice(0, i)}` takes the
roots of its view arguments. A carrier follows the same escape rules as a view (field, container, return of a local
owner, outer binding of an inner owner) and the same `[returns_borrow<name>]` rule when it is returned. Parsing into a
carrier allocates nothing: the `Request` parser test counts zero heap blocks. A closure is a carrier when it captures a
view or carrier by value or any binding with `[ref name]` (value-lifecycle.md, Lambdas): `reader{[view]() {...}}`
borrows the view's owner until `reader`'s last use, and such a closure cannot become a `Function<R, A...>` (`closure
escapes via Function (root: <root>)`).

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
`reverse`, `copyFrom`, `sort`. A `vector<T>` has the method spellings `values.view()`, `values.slice(start, end)` and
`values.sliceMut(start, end)`, and a view indexes with `view[i]` and `view[i] = v` (the struct operator rewrite calls the
`opAt` and `opSet` members of the slice structs). An `array<T>` has the same three method spellings; they call
`arrayView`, `arraySlice` and `arraySliceMut` in `/std/collections/slice`, which take the array by reference and build
the view from `location(at_unsafe(values, 0))`, the address of the first element slot (`location(at(values, i))` is
range checked). The method form of `slice` on an array is therefore a view, not a copy; the free function form
`slice(values, start, end)` still copies (TODO-5654).

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
  to one is rejected (`string field requires static text; use String`). In the shipped design the borrowed form is
  `StringView`, and a `string` can only be a literal or an entry argument, so every `string` is static text by
  construction and the field rule needs no diagnostic: a `string` field can be assigned any `string`, and text that
  must outlive a buffer is a `String` field.
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
- **String heap:** longer text comes from a heap of chunks divided into size classes, four per doubling from 32
  bytes to 64 KiB, so at most about 25% of a block is unused. A class's first chunk is 4 KiB (one block, for blocks
  over 4 KiB) and each further chunk is as large as the class's chunks together, up to 256 KiB, so a program with a
  few short strings holds a few KiB and one with many takes large chunks. A freed block goes back on its class's
  free list and is reused. Of the chunks whose blocks are all free, each size class keeps one (the smaller of two)
  and the heap keeps at most 1 MiB, returning the largest first; the others are returned to the system. That is the
  bound on what a heap holds once every string is dropped: at most one chunk per size class used and at most 1 MiB
  (12 KiB after strings that stayed under 128 bytes). A string that grows through several block sizes and is
  dropped, in a loop, therefore takes no new chunk after the first round. The memory limit
  (`/std/text/setStringMemoryLimit`) counts the chunks and large blocks held, and free chunks are returned before
  an allocation is refused. The first chunk is made on the first long string, so programs that never need one pay
  nothing.
- **Large text:** text over 64 KiB gets its own allocation, returned to the system as soon as it is freed.
- **Growth:** capacity doubles, rounded up to the size class; growing within a class does not move the text.
  `reserve(n)` gives exactly the class that holds `n`; `shrinkToFit()` moves the text to the smallest class that holds
  it.
- **Observability:** `stringMemory()` returns `StringMemoryStats { chunks, bytesInUse, bytesFree, largeBlocks,
  largeBytes }`. `setStringMemoryLimit(bytes)` caps the heap; an allocation past the cap is the runtime fault
  `string memory limit exceeded`, and `String.tryReserve(n)` returns `Result<void, TextError>` instead of faulting.
- **Threads:** each thread has its own string heap; a `String` freed on another thread returns its block to the heap
  it came from.
- **Arenas:** `[StringArena] arena{beginStringArena()}` opens a string arena that ends when `arena` goes out of scope.
  While it is open every `String` block is bump-allocated from regions of the arena (4 KiB at first, doubling to 256 KiB
  per size class; large blocks get a region of their own), freeing a block does nothing, and ending the arena returns
  all its regions at once: O(1) per region, no per-string free. Arenas nest (the innermost one is used), the memory
  limit counts their regions, `stringArenaDepth()` and `stringArenaBytes()` report them, and `stringHeapAllocations()`
  counts blocks. Text made in an arena must not outlive it: the checker rejects returning such a `String`, assigning it
  to a binding made before the arena and pushing it into a container made before it (`string escapes its arena via
  return|outer binding|container`) (a `SharedString` made in an arena has its text there and is checked the same way), and any other use of the text after the arena ended faults, because its addresses
  are invalid (releasing it reports `invalid string heap free address`). A `String` that is grown while an arena is open
  takes its new block from the arena, so a `String` made before the arena should not be grown inside it and kept.
  Measured at the allocator (`test_vm_text_heap.cpp`): 400 blocks of five size classes, taken and returned, run about 13
  times faster in an arena than through the heap; in the language-level request benchmark
  (`benchmarks/strings/request_arena.prime` against `request_heap.prime`) the two run alike, because `String`
  operations and the `vector` dominate there.

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
  `stringHeapLargeBlocks()`, `stringHeapLargeBytes()` and `setStringMemoryLimit(bytes)`; `heapLiveBlocks()` counts
  the live allocations of the slot heap (vector buffers, `Function` boxes) on the VM and optexe, so a test can check
  that a copy-and-destroy loop frees what it makes. `text` parameters are the
  current literal-backed `string`, read byte by byte; they become real views with TODO-5540.
- `pushCodePoint(point)` appends the UTF-8 encoding of a code point (a surrogate or a value past U+10FFFF stops the
  program); `+` and the comparisons are the operators described under the text view below.
- Not yet: `+=` (the language has no token), the `Result`-returning `tryReserve` and per-thread heaps. A `String` is never borrowed implicitly where a view is expected: write `s.view()`, so every borrow is visible.

## Parity with `std::string` and `std::string_view`

Quality bar: `String` and `string` are no less capable than `std::string` and `std::string_view`, with UTF-8 text
operations on top. Every C++ member has a PrimeStruct spelling or a stated difference. "Done" means on the VM today;
the other entries are specified here and tracked by the TODO named.

| C++ | PrimeStruct | Status |
| --- | --- | --- |
| `string()`, `string(const char*)`, `string(sv)` | `String{}`, `stringFrom("text")`, `view.toOwned()` | done |
| `string(n, ch)`, `assign(n, ch)` | `stringFilled(n, byte)`, `assignFilled(n, byte)` | done |
| `string(sv, pos, n)`, `substr` | `view.slice(a, b).toOwned()`, `substr(a, b)` | done |
| construct from raw bytes | `textFromBytes(bytes)` (valid UTF-8 or stops), `stringFromUtf8Lossy(bytes)` | done |
| copy / move, destructor | `Copy`, `Move`, `Destroy` lifecycle | done |
| `at`, `operator[]` | `byteAt(i)`; `s[i]` | done: `s[i]` (`opAt`) |
| `front`, `back` | `front()`, `back()` (stop on empty text) | done |
| `data`, `c_str` | the block, always NUL terminated | done |
| `operator string_view` | `s.view()` (explicit; no implicit borrow) | done; loans TODO-5569 |
| `begin`/`end` iterators | `bytes()`, `codePointAt`/`codePointLength`, index loops (range-`for` is a separate language change) | done |
| `empty`, `size`, `capacity`, `reserve`, `shrink_to_fit`, `clear` | `empty`, `count`, `capacity`, `reserve`, `shrinkToFit`, `clear` | done |
| `resize` | `resize(n, byte)` (a shrink inside a code point stops) | done |
| `push_back`, `pop_back` | `pushByte`, `popByte`, `pop()` (a code point); `pushCodePoint` | done |
| `append(sv)`, `append(n, ch)`, `operator+=` | `append("lit")`, `appendView`, `appendBytes`, `appendFilled`; `+=` | `a = a + b` done; `+=` has no token in the language |
| `insert`, `erase`, `replace` | `insert`, `erase`, `replace` (byte ranges), `insertFilled`, `insertByte`, `replaceFilled` | done |
| `operator+` | `a + b` | done (`opPlus`); chains of three or more are one allocation (`opPlusMany`) |
| `swap` | `swap(other)` | done |
| `copy(dst, n, pos)` | `view.copyTo(destination)` | done |
| `compare`, `==`, `<=>` | `compare(other)`, `equals`; `==` and `<` operators | done (`opEqual`, `opLessThan`, ...) |
| `starts_with`, `ends_with`, `contains` | `startsWith`, `endsWith`, `contains` | done |
| `find`, `rfind` (with `pos`) | `indexOf`, `indexOfFrom`, `lastIndexOf` (-1 when absent; `Maybe` TODO-5642) | done |
| `find_first_of`, `find_last_of`, `find_first_not_of`, `find_last_not_of` | `findFirstOf`, `findLastOf`, `findFirstNotOf`, `findLastNotOf` (+ `...From`) | done |
| `to_string`, `stoi`, `stol`, `stod`, `from_chars` | `stringFromI32/I64/U64/F64/...`, `parseI32Into/I64Into/U64Into/F64Into` and `...Or` | done |
| `to_chars` (radix, precision, shortest round trip) | `appendI64Radix`, `appendF64`, `appendF64Fixed`, `appendF64Scientific`, `appendF32` | done |
| `std::hash<string>`, `unordered_map<string, V>` | `hash()`, `hashSeeded(seed)` (SipHash-1-3), `StringMap<V>` | done; builtin `map<String, V>` is not hashed |
| `std::format`, `operator<<`, `getline` | `formatText(pattern, args...)`, `joinWith`, `lineAt`, `splitOwned`, `readTextInto` | done |
| `memcpy`/`memmove`/`memcmp`/`memchr` speed | the `__pstext_move/fill/compare/find` host functions under every copy, compare and search | done |
| `string_view::remove_prefix`/`remove_suffix`/`substr` | `slice` | done |

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
| Request-scoped string arenas | all of a request's text freed in O(1) | done (`StringArena`) |
| Fused `+` chains and one-allocation `format` | one block per expression, guaranteed | done (`opPlusMany`, `stringHeapAllocations()` counts blocks); `format` sizing TODO-5627 |
| `Maybe`/`Result` APIs | no `npos`, no exceptions, no undefined behavior | TODO-5619, TODO-5620 |
| Linear-time search | `find` never goes quadratic | TODO-5619 |
| Byte-identical text on every backend | locale-free number and case output | TODO-5626 |
| Keyed hash, `Symbol` interning | flood-resistant keys; O(1) equality for names | TODO-5625, TODO-5636 |
| UTF-8 invariant, Unicode case, normalization, graphemes | correct text, loaded only when used | done (`/std/text/unicode`, `/std/text/unicode_norm`) |
| `Secret<String>` | zeroed on destroy, never printed, constant-time compare | TODO-5635 |
| Compile-time folding of literal operations | no run-time work for constants | done: `+`, `==`, `!=`, `at` on literals; `==`, `<`.., `count`, `startsWith`, `endsWith`, `contains`, `[i]` on `textOf("..")`; a literal index out of range is a compile error |
| `SharedString` | cheap immutable sharing: copying adds one to an atomic count, the last owner frees (`/std/text/shared`); sendable, so the owners may be on different threads (VM and `optexe`). The count is the 8 bytes before the text in the same string block, so making one takes one allocation. Made in a `StringArena` it follows the arena rules of a `String` | done |

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
took `insert_front`, `copy_text` and `replace_middle` from 20 to 60x to about 1 to 2x. Appending a string literal is one bulk move from
the literal's cached byte region (`__pstext_literal`, the address of the literal's bytes, copied once per literal and
thread heap) since TODO-5840: `append_chunks` retires 0.35e9 instructions where it retired 1.26e9, `short_strings`
3.90e9 where it retired 4.66e9. What is left: `replace` and `equalsText` still copy or compare a literal byte by byte
(two host calls cost more than the loop for a few bytes), and every non-empty `String` takes a heap block
(`short_strings`). Lower a budget whenever its ratio drops.

Found by the suite: a binding initialized from another `String` binding aliases the heap block and frees it twice
(TODO-5639); the supported way to duplicate text is a `[String copy]` parameter. Known limits: counts are `i32`, so text
is capped at 1 GiB (`reserve` rejects more).

### Current surface of the text view (VM and optexe)

The view is built in stages. `string` is still the static literal type (a module string-table index): print, map
keys, `File` paths, host parameters and every existing program keep it. Text that is not a literal is viewed through
the stdlib struct `StringView` (`import /std/text/view/*`, a module of its own; the name follows `std::string_view`), a
pointer to the first byte and a byte count. A program that only makes `String`s imports `/std/text/*` and does not pay
to compile the view code. `TextView` is taken by the native-UI widget.

- **Making one:** `s.view()` on a `String`, a literal where a view is expected or `textOf("literal")`, `s.slice(a, b)` or `v.slice(a, b)`
  for part of a text, `textFromBytes(bytes)` for bytes that are valid UTF-8. A literal's bytes get an address from the
  text host function `__pstext_literal` (a NUL-terminated byte region made the first time and kept for the run); an
  empty literal or empty String gives the null pointer and a count of 0.
- **A `string` where a view is expected:** a string literal, or any `string` value, converts to a `StringView` where
  one is expected, as if written `textOf(value)`: a call or method-call argument for a `[StringView]` parameter
  (each value of an `[args<StringView>]` parameter too), a brace-constructor argument for a `StringView` field (of a
  generic struct too, for a field whose type does not name a template parameter), the
  initializer of a `[StringView]` binding, an assignment to a `StringView` local or parameter, and the value returned
  from a definition that returns a `StringView`. Template monomorphization inserts the `textOf` call once it knows the
  declared type, so the program lowers exactly as if the call were written out. Nothing else converts: a `String`
  still needs `s.view()`, and a non-text argument is the usual `argument type mismatch ... expected
  /std/text/view/StringView got i32`. At a call of same-arity overloads the literal converts only when every overload
  takes a `StringView` in that position, so an overload that takes the `string` itself is the exact match and wins,
  and one that takes something else leaves the literal as a `string`.

  ```prime
  import /std/text/view/*

  [struct]
  Label {
    [StringView] text{StringView{}}
  }

  [return<i32>]
  width([StringView] text) {
    return(text.count())
  }

  [return<StringView>]
  fallback() {
    return("none")
  }

  [return<int>]
  main() {
    [string] word{"four"}
    [StringView] bound{"seven"}
    [Label] label{Label{[text] word}}
    // 5 + 5 + 4 + 4
    return(width("hello") + bound.count() + label.text.count() + fallback().count())
  }
  ```

  The program exits with 18 on the VM and through `--emit=optexe` (the native backends have no `StringView` yet).
- **A literal that tasks may share:** a `StringView` is not sendable, because it may point into a `String` that
  another task frees. `literalTextOf("literal")` gives a `LiteralText`, a sendable struct that can only come from a
  static `string` (whose bytes live for the whole run), and `view()` on it gives the `StringView`. The route tables
  of `/std/http` and `/std/web` keep their methods, patterns and control ids in it, which is what lets a whole app
  sit in a `Mutex` (`/std/http/server` `serveWorkers`).
- **Access:** `count()`, `empty()`, `byteAt(i)` (faults outside the text), `bytes()` (a `ReadSlice<u8>`), `isBoundary(i)`.
  A slice cut inside a code point, or outside the text, stops the program.
- **Compare and search** (byte order, which orders UTF-8 by code point): `equals`, `equalsText`, `compare`,
  `startsWith`, `endsWith`, `contains`, `indexOfByte`, `indexOf`, `indexOfFrom`, `lastIndexOf` (an index or -1), and `find`, `rfind` as a `Maybe<i32>`. Forward
  search is linear time (two-way matching, no allocation). The bulk work is done by the `__pstext_*` functions of
  the TODO-5629 section.
- **Pieces:** `trim`, `trimStart`, `trimEnd` (ASCII white space), `splitCount`/`splitPart(separator, n)` (views),
  `lineCount`/`lineAt(n)` (a carriage return before the line feed is dropped; no empty line after a final one).
- **Numbers:** `parseI32Into(out)`, `parseI64Into(out)`, `parseF64Into(out)`, the `...Or(fallback)` forms, and `parseI32()` as a `Maybe<i32>` (`Maybe` has `isSome`, `isEmpty` and `unwrapOr`; `first()` and `last()` give a `Maybe<u8>`); the text
  must be exactly a number in range (floats: a plain decimal, no white space, hexadecimal, inf or nan), and `out` is
  left alone on failure. The float conversion is the C library's, correctly rounded (`__pstext_parse_f64`).
- **Code points and case:** `codePointCount()`, `codePointLength(offset)`, `codePointAt(offset)` (an i32), and
  `toAsciiLower()`/`toAsciiUpper()` (a `String`) and `equalsIgnoreAsciiCase(other)`; bytes 128 and up are left alone.
- **Checking bytes:** `utf8ErrorOffset(data, count)` is the offset of the first byte that starts invalid UTF-8, or -1.
- **Search by byte set, replace:** `findFirstOf`, `findFirstNotOf`, `findLastOf`, `findLastNotOf` (and `...From(set, pos)`, with
  `std::string`'s position meaning) return an index or -1; `replaceAll(from, to)` gives a `String`; `toOwned()` copies a
  view into a `String`; `splitOwned(separator)` gives a `vector<String>`.
- **Formatting:** `formatText(pattern, args...)` takes views and follows `std::format`: `{}`, `{N}`, `{{`, `}}` and
  `{:[[fill]align]width}`; `joinWith(separator, parts...)`. Numbers are rendered first with `stringFromI64` and friends
  (radix 2 to 36; floats as `std::to_chars` writes them) and passed as views.
- **Files:** `readTextInto(path, out)`, `writeText(path, text)`, `appendText(path, text)` take a view as the path and
  return 0 or the C errno (`readTextInto` also -1 for invalid UTF-8); they need the `file_read` / `file_write` effects.
- **Hashing and keys:** `hash()` and `hashSeeded(seed)` on a view, `hash()` on a `String` (SipHash-1-3, a 64-bit seed, 0 by
  default; the same value on every backend). `import /std/text/string_map/*` gives `StringMap<V>`, a hash table
  from text keys to Copy values: `insert`, `at`, `getOr`, `tryGet`, `contains`, `indexOf`, `erase`, `clear`, `count`,
  `keyAt(i)`, `valueAt(i)`, `setSeed(seed)`. Pass a secret random seed to `setSeed` to make collisions unpredictable.
- **Interned names:** `import /std/text/interner/*` gives `Interner` and `Symbol` (an i32 id in first-seen order).
- **Secrets:** `import /std/text/secret/*` gives `Secret`, a text that cannot be printed or formatted, compared with
  `equalsConstantTime`, read with `reveal()`, and zeroed on destroy. Field privacy is not enforced by the compiler yet.
- **Compile cost:** a module is spliced in whole, so `/std/text` (the `String` core) and `/std/text/view` (`StringView`,
  search, parsing, formatting, files, UTF-8 checks) are separate import roots in `stdlib/std/modules.psmeta`. A program
  that makes one `String` compiles in 0.15 s (it was 0.12 s before the view code, 0.50 s when both were one module).
  `s.view()` and `s.slice(a, b)` are members of `String` defined in the view module, and `appendView(out, view)` is a
  function there.
- A view is valid while its text is not changed or freed. The loan rules that make that a compile-time guarantee are
  TODO-5569; until then it is the program's responsibility, as with `ReadSlice`.
- Unicode case (`import /std/text/unicode/*`): `view.toLower()`, `view.toUpper()` and `view.caseFold()` return a new
  `String` with the full default mappings applied code point by code point (a sharp s upper-cases to `SS`, `U+0149` to
  `U+02BC U+004E`, ligatures expand), `view.equalsIgnoreCase(other)` compares after case folding, and `toLowerText`,
  `toUpperText`, `caseFoldText` are the same as functions. The rules are the unconditional ones, locale free: no final
  sigma, no Turkish dotted i, no title case. The tables (Unicode 15.0, generated by `scripts/generate_unicode_case.py`
  into `stdlib/std/text/unicode_data.prime`) are a separate module that is part of a program only when it calls one of
  these, so a program that does not use them carries none of the table bytes. Invalid UTF-8 stops the program.
- Unicode normalization and graphemes (`import /std/text/unicode_norm/*`): `view.normalizeNfc()` and
  `view.normalizeNfd()` return a new `String`, `view.isNfc()` tells whether the text is already NFC, and
  `view.graphemeCount()` and `view.graphemeLength(offset)` walk the extended grapheme clusters (UAX #29: CR LF, Hangul
  syllables, extending and spacing marks, prepended marks, emoji ZWJ sequences, regional-indicator pairs); a loop over
  `graphemeLength(offset)` gives each cluster as a slice. NFKC/NFKD and word or sentence breaks are not provided. The
  tables (generated by `scripts/generate_unicode_text.py` from Python's `unicodedata` and perl's `Unicode::UCD`, both
  Unicode 15.0) are a separate module that is part of a program only when it calls one of these.
- Operators: `a + b` (a new `String`), `==`, `!=`, `<`, `<=`, `>`, `>=` and `s[i]` work on `String` and on `StringView`
  (byte order; `s[i]` is the byte, stopping when out of range). The compiler rewrites an operator call whose first operand is
  a struct value, and whose second operand has the same type, to the struct's member `opPlus`, `opEqual`, `opNotEqual`,
  `opLessThan`, `opLessEqual`, `opGreaterThan`, `opGreaterEqual` or `opAt` (`opPlus` and the others are ordinary members
  defined as `/Type/opPlus([Type] self, [Type] other)`; a user struct gets operators the same way). The `op` prefix is
  deliberate: a member named `plus` would capture every bare `plus(a, b)` inside the struct. Operands must be names, calls of
  free functions or operator results; bind a field or method-chain result to a local first. `+=` has no token yet, write
  `a = a + b`.
- Not yet: `Result<..., TextError>` results (the result lane, TODO-5524 to TODO-5526), the float functions on the native backends (the arm64 native backend answers
  `__pstext_literal` since TODO-5840, with the address of the literal's bytes in the image's string table, for
  `String.append`; the bytes are not NUL terminated there and StringView over it is untested), and unifying `string`
  and `StringView` into the single type described above (TODO-5621).

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
  compiler reports `run-time text requires String` where a `string` would need to own its text: a `[host return<string>]`
  declaration is rejected with that message (declare `[host return<String>]`).
- `Slice<T, Capability>` parameters that today receive a copied `array<T>` receive a real slice; code that relied on
  the copy being independent of its source keeps working because the source cannot change while the slice is live.

## Implementation slices

| TODO | Slice |
| --- | --- |
| TODO-5537 (done) | IR byte opcodes (`LoadU8`/`StoreU8`/`HeapAllocBytes`/`ZextU8`) and VM byte regions |
| TODO-5565 (done) | the `u8` scalar type |
| TODO-5566 (done), TODO-5567 (done) | packed byte containers: `vector<u8>` and `Pointer<u8>` buffers, then `array<u8>` (spelled as `vector<u8>`) |
| TODO-5538 (done), TODO-5569, TODO-5570 | `Slice<T, Capability>` as a real view with shared operations; loan and escape rules; method spellings and `array<T>` views |
| TODO-5539 (done) | `String`: value, lifecycle, the string heap and the owned-text API |
| TODO-5540 | `StringView`: a borrowed view of literals and Strings |
| TODO-5618 | `string` borrowed from `String`, `view()`, `slice()`, `utf8()` |
| TODO-5619 | `string` search, ordered comparison, trim, `split`, `lines` |
| TODO-5620 | `string` parsing, code points, ASCII case |
| TODO-5621 | static-text field rule and `run-time text requires String` |
| TODO-5622 | optimized C++ emitters (`optcpp`, `optexe`) run the view |
| TODO-5630 to TODO-5638 | beyond `std::string`: arenas, view-holding structs, fused concatenation, Unicode, `Secret`, `Symbol`, literal folding, `SharedString` (see Beyond above) |
| TODO-5623 to TODO-5629 | `std::string` parity: mutation, search sets, hashing and map keys, number formatting, `format`/`join`, conformance and speed gate, bulk byte opcodes (see Parity above) |
| TODO-5530 | Read and write whole files as `String` |
| TODO-5541 | Host functions and the embedding API use `String`/`string`; retire VM run-time string indices |
| TODO-5574 | Simple C++ emitters for `u8`, slices, `string` and `String` (they hand such programs to the optimized emitter) |
