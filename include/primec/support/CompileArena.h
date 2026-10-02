#pragma once

// TODO-5233/TODO-5234/TODO-5235: scoped, resettable arena allocator used by
// the CLI binaries (primec/primevm). TODO-5235 built the general escape
// hatch below (SystemHeapScope / systemHeapValue)
// so that resets *could* also be made safe inside the long-lived doctest
// test binaries (semantics/ir_pipeline/etc.), but that per-TEST_CASE reset
// wiring has NOT shipped as of this writing - see
// docs/CompilerArenaAllocator.md's "TODO-5235" section and
// tests/unit/test_main.cpp's own file comment for why (three
// fix-rebuild-rerun rounds each found a *different* magic-static hazard
// class than the last, the opposite of the search converging). Test
// binaries currently run entirely on the system allocator, exactly as
// TODO-5234 shipped it; do not assume otherwise from this file alone.
//
// See docs/CompilerArenaAllocator.md for the full design writeup: the
// allocation survey, an earlier "reset per compile" design that turned out
// to be unsafe (magic-static corruption), and the general escape hatch
// (SystemHeapScope / systemHeapValue) that
// TODO-5235 added to make resets safe *when nothing arena-allocates a
// process-lifetime value*. Summary of the public contract:
//
//   - ScopedCompileArena is an RAII guard marking "the arena is active for
//     the duration of this scope." CLI binaries construct exactly one, near
//     the top of main(), living for the whole process, and it never resets
//     in practice since the process exits before it goes out of scope. A
//     future per-TEST_CASE construction in test binaries (not yet wired -
//     see above) would be the reset path this mechanism was built for.
//     Nesting is supported via a thread_local depth counter; when the
//     outermost scope on a thread ends, that thread's arena is reset (bump
//     cursor and free lists rewound to empty - the chunks themselves are
//     kept and reused, not unmapped).
//   - While at least one ScopedCompileArena is alive on a thread, that
//     thread's small (<=4096-byte), default-alignment heap allocations are
//     served from a thread-local bump/free-list arena instead of glibc
//     malloc - UNLESS a SystemHeapScope is also active on that thread (see
//     below), in which case they still go to the system allocator. Larger
//     or over-aligned allocations, and allocations on threads that never
//     enter a scope (e.g. std::async validation workers), always fall
//     through to the ordinary system allocator.
//   - SystemHeapScope is the general escape hatch TODO-5235 added: while
//     one is alive on a thread, allocations on that thread are forced to
//     the system heap regardless of whether a compile scope is active. Use
//     it (or the systemHeapValue() convenience wrapper) to build the value
//     of any function-local "magic static" - a `static const std::string`/
//     `std::vector`/etc. computed once on first call and expected to live
//     for the rest of the process - so its backing heap buffer is never
//     arena-allocated and therefore never at risk of being silently
//     reclaimed by a later arena reset while the static is still alive.
//     Every known instance of this pattern under src/semantics,
//     src/ir_lowerer, and src/parser has been wrapped this way; wrap any
//     new one the same way rather than inventing a bespoke fix.
//   - Persistent caches (TODO-5359/5383): state that outlives a single call
//     lives in the per-compilation CompileContext (primec/support/CompileContext.h),
//     which is destroyed with the compilation, so no cache entry can dangle
//     into memory an arena reset reclaimed. There is no reset-callback registry
//     any more.
//   - operator delete is always safe to call on any pointer, regardless of
//     which thread freed it or whether an arena is currently active on that
//     thread: every allocation carries a small header identifying how (and,
//     if applicable, by which arena) it was allocated, so delete never
//     guesses from ambient state.

namespace primec {

// RAII guard marking "the arena is active for this scope." Construct one
// per compile (CLI: once near the top of main(), for the whole process;
// test binaries: once per TEST_CASE). When the outermost instance on a
// thread is destroyed, that thread's arena resets - see the file comment above.
class ScopedCompileArena {
public:
  ScopedCompileArena();
  ~ScopedCompileArena();

  ScopedCompileArena(const ScopedCompileArena &) = delete;
  ScopedCompileArena &operator=(const ScopedCompileArena &) = delete;
  ScopedCompileArena(ScopedCompileArena &&) = delete;
  ScopedCompileArena &operator=(ScopedCompileArena &&) = delete;
};

// RAII guard: for as long as this is alive on the current thread, every
// allocation on that thread is served from the system heap even if a
// ScopedCompileArena is also active. See the file comment above - this is
// the general mechanism for keeping process-lifetime "magic static" values
// safe under arena resets.
class SystemHeapScope {
public:
  SystemHeapScope();
  ~SystemHeapScope();

  SystemHeapScope(const SystemHeapScope &) = delete;
  SystemHeapScope &operator=(const SystemHeapScope &) = delete;
  SystemHeapScope(SystemHeapScope &&) = delete;
  SystemHeapScope &operator=(SystemHeapScope &&) = delete;
};

// Convenience wrapper: evaluate `f` (typically a lambda that builds a magic
// static's value) with a SystemHeapScope active, so every allocation it
// performs - including nested ones, e.g. a std::string built up inside a
// std::vector<std::string> initializer - comes from the system heap.
//
// Usage: `static const T x = primec::systemHeapValue([] { ...; return v; });`
template <typename F>
auto systemHeapValue(F &&f) -> decltype(f()) {
  SystemHeapScope guard;
  return f();
}

} // namespace primec
