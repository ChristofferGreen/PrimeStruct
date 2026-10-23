#pragma once

#include <string_view>

namespace primec {

// The string heap functions (`__pstext_*`, see primec/runtime/VmTextHeap.h) the arm64 native backend
// answers itself with emitted code. A text block is a byte region of its own sized by the VM's size
// classes; the pooled heap, statistics, limit and entry arguments are not there yet (docs/todo.md).
enum class NativeTextHost {
  NotText,     // not a `__pstext_*` function
  Alloc,       // __pstext_alloc(size) -> Pointer<u8>
  TryAlloc,    // __pstext_try_alloc(size) -> Pointer<u8> (null on failure)
  Free,        // __pstext_free(Pointer<u8>)
  Capacity,    // __pstext_capacity(Pointer<u8>) -> u64
  Length,      // __pstext_length(Pointer<u8>) -> i32
  Usable,      // __pstext_usable(Pointer<u8>) -> u64
  Argument,    // __pstext_argument(i32) -> Pointer<u8>: the entry argument at the index in a fresh block
  Unsupported, // a `__pstext_*` function the native backend does not answer yet
};

// The function an import names; the stdlib declares them inside a namespace, so the import name may
// carry a path.
inline NativeTextHost nativeTextHost(std::string_view importName) {
  const size_t slash = importName.rfind('/');
  const std::string_view leaf = slash == std::string_view::npos ? importName : importName.substr(slash + 1);
  if (leaf.rfind("__pstext_", 0) != 0) {
    return NativeTextHost::NotText;
  }
  if (leaf == "__pstext_alloc") {
    return NativeTextHost::Alloc;
  }
  if (leaf == "__pstext_try_alloc") {
    return NativeTextHost::TryAlloc;
  }
  if (leaf == "__pstext_free") {
    return NativeTextHost::Free;
  }
  if (leaf == "__pstext_capacity") {
    return NativeTextHost::Capacity;
  }
  if (leaf == "__pstext_length") {
    return NativeTextHost::Length;
  }
  if (leaf == "__pstext_usable") {
    return NativeTextHost::Usable;
  }
  if (leaf == "__pstext_argument") {
    return NativeTextHost::Argument;
  }
  return NativeTextHost::Unsupported;
}

} // namespace primec
