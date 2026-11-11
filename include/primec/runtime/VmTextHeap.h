#pragma once

#include <algorithm>
#include <cerrno>
#include <charconv>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "primec/runtime/VmHeapCore.h"

namespace primec {

// The string heap that backs `String` (docs/spec/strings-and-views.md, Memory). Text up to
// LargeThreshold bytes comes from chunks, each divided into equal blocks of one size class (four
// classes per doubling from 32 bytes to 64 KiB, so at most about a quarter of a block is unused);
// longer text gets a byte region of its own. A class's first chunk is 4 KiB (one block, for a class
// with larger blocks) and each further chunk is as large as the chunks the class holds together, up
// to 256 KiB. Chunks and large blocks are byte regions of the VM heap, so a block address works with
// LoadU8/StoreU8 and address + i is byte i.
//
// A freed block goes back on its chunk's free list. Of the chunks whose blocks are all free, each
// size class keeps one (the smaller of two, so a class that shrinks ends with its first chunk) and
// the heap keeps at most FreeChunkLimit bytes (1 MiB, more than the first chunks of all classes
// together), returning the largest first; the others are returned to the VM heap. So text that
// grows through several block sizes and is dropped, again and again, takes no new chunk, and a heap
// whose blocks are all free holds at most 1 MiB (12 KiB after strings of up to 127 bytes). A large
// block is returned as soon as it is freed. The heap has an optional byte limit over the chunks and
// large blocks it holds; free chunks are returned before an allocation is refused for it.
//
// The class body lives in VmTextHeapBody.inc and is expanded twice: here as code (primec::TextHeapStats,
// primec::VmTextHeap, primec::callTextHostFunction) and as the source text primec::VmTextHeapSource, which the
// optimized C++ backend pastes into generated programs (after VmHeapCoreSource) so their string heap behaves
// exactly like the VM's.
#define PRIMEC_VM_TEXT_HEAP_BODY(...) __VA_ARGS__
#include "primec/runtime/VmTextHeapBody.inc"
#undef PRIMEC_VM_TEXT_HEAP_BODY

#define PRIMEC_VM_TEXT_HEAP_BODY(...) inline constexpr const char *VmTextHeapSource = #__VA_ARGS__;
#include "primec/runtime/VmTextHeapBody.inc"
#undef PRIMEC_VM_TEXT_HEAP_BODY

// The host functions the VM answers itself (the stdlib `String` declares them with `[host]`):
//   __pstext_alloc(size) -> Pointer<u8>      __pstext_try_alloc(size) -> Pointer<u8> (null on failure)
//   __pstext_free(Pointer<u8>)               __pstext_capacity(Pointer<u8>) -> u64 (block size)
//   __pstext_stat(which) -> u64              0 chunks, 1 bytesInUse, 2 bytesFree, 3 largeBlocks, 4 largeBytes, 5 allocations, 6 open arenas, 7 arena bytes
//   __pstext_literal(string) -> Pointer<u8>: the address of a string literal's bytes (VM and optexe)
//   __pstext_f64_ok(address, count) -> i32 / __pstext_parse_f64(address, count) -> f64: decimal text to double
//   __pstext_move(dst, src, count) / __pstext_fill(dst, value, count) / __pstext_compare(a, b, count) -> i32 /
//   __pstext_find(address, count, value) -> i32: bulk byte operations, one range check per call (VmHeapCore::byteSpan)
//   __pstext_length(Pointer<u8>) -> i32 (text length)   __pstext_usable(Pointer<u8>) -> u64 (block size less the NUL)
//   __pstext_set_limit(bytes)
//   __pstext_arena_begin() / __pstext_arena_end(): open and close a string arena (the stdlib `StringArena`)
inline constexpr std::string_view TextHostPrefix = "__pstext_";

// The `__pstext_*` function an import names (the stdlib declares them inside a namespace, so the import
// name carries a path), or empty when the import is not one of them.
inline std::string_view textHostFunctionName(std::string_view importName) {
  const size_t slash = importName.rfind('/');
  const std::string_view leaf = slash == std::string_view::npos ? importName : importName.substr(slash + 1);
  return leaf.rfind(TextHostPrefix, 0) == 0 ? leaf : std::string_view{};
}

} // namespace primec
