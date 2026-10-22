#pragma once

#include <algorithm>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "primec/runtime/VmHeapCore.h"

namespace primec {

// The string heap that backs `String` (docs/spec/strings-and-views.md, Memory). Text up to
// LargeThreshold bytes comes from 256 KiB chunks, each divided into equal blocks of one size class
// (four classes per doubling from 32 bytes to 64 KiB, so at most about a quarter of a block is
// unused); longer text gets a byte region of its own. Chunks and large blocks are byte regions of
// the VM heap, so a block address works with LoadU8/StoreU8 and address + i is byte i.
//
// A freed block goes back on its chunk's free list. A chunk whose blocks are all free is returned to
// the VM heap, except one kept for reuse; a large block is returned as soon as it is freed. The heap
// has an optional byte limit over the chunks and large blocks it holds.
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
//   __pstext_stat(which) -> u64              0 chunks, 1 bytesInUse, 2 bytesFree, 3 largeBlocks, 4 largeBytes
//   __pstext_length(Pointer<u8>) -> i32 (text length)   __pstext_usable(Pointer<u8>) -> u64 (block size less the NUL)
//   __pstext_set_limit(bytes)
inline constexpr std::string_view TextHostPrefix = "__pstext_";

// The `__pstext_*` function an import names (the stdlib declares them inside a namespace, so the import
// name carries a path), or empty when the import is not one of them.
inline std::string_view textHostFunctionName(std::string_view importName) {
  const size_t slash = importName.rfind('/');
  const std::string_view leaf = slash == std::string_view::npos ? importName : importName.substr(slash + 1);
  return leaf.rfind(TextHostPrefix, 0) == 0 ? leaf : std::string_view{};
}

} // namespace primec
