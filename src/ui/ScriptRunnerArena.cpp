// The compiler's arena without its global operator new/delete, for libps_script (see CompileArena.cpp): linked before
// the support library, this object satisfies the arena symbols so the library's copy is not pulled in. Because the
// library's own operators are therefore not found either, the system ones are defined here: plain malloc and free, the
// same allocator libc++.dylib's own string and container code uses in a host program that does not replace the
// operators (an app does not; a test program that links the compiler runs the runner directly instead, see
// primec::ui::setScriptRunnerApi).
#define PRIMEC_NO_GLOBAL_OPERATOR_NEW 1
#include "../support/CompileArena.cpp"

#include <cstdlib>
#include <exception>
#include <new>

void *operator new(std::size_t size) {
  if (void *block = std::malloc(size == 0 ? 1 : size)) {
    return block;
  }
  std::terminate(); // out of memory: operator new may neither return null nor throw here (no exceptions in the sources)
}

void *operator new[](std::size_t size) { return ::operator new(size); }

void *operator new(std::size_t size, const std::nothrow_t &) noexcept { return std::malloc(size == 0 ? 1 : size); }

void *operator new[](std::size_t size, const std::nothrow_t &) noexcept { return std::malloc(size == 0 ? 1 : size); }

void operator delete(void *ptr) noexcept { std::free(ptr); }

void operator delete[](void *ptr) noexcept { std::free(ptr); }

void operator delete(void *ptr, std::size_t) noexcept { std::free(ptr); }

void operator delete[](void *ptr, std::size_t) noexcept { std::free(ptr); }

void operator delete(void *ptr, const std::nothrow_t &) noexcept { std::free(ptr); }

void operator delete[](void *ptr, const std::nothrow_t &) noexcept { std::free(ptr); }
