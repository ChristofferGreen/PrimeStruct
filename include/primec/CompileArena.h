#pragma once

// TODO-5233/TODO-5234: process-lifetime arena allocator for the CLI
// binaries (primec/primevm) only.
//
// See docs/CompilerArenaAllocator.md for the full design writeup: the
// allocation survey, an earlier "reset per compile" design that turned out
// to be unsafe (magic-static corruption - see CompileArena.cpp's file
// comment for the mechanism), and why this narrower variant is the one
// that shipped. Summary of the public contract:
//
//   - ScopedCompileArena is an RAII guard. Exactly one is constructed, at
//     CLI startup (main.cpp / primevm_main.cpp), and lives for the rest of
//     the process. Nesting is supported (a thread_local depth counter) but
//     is not expected to matter in practice given there is only ever one
//     top-level call site per binary.
//   - While at least one ScopedCompileArena is alive on a thread, that
//     thread's small (<=4096-byte), default-alignment heap allocations are
//     served from a thread-local bump/free-list arena instead of glibc
//     malloc. Larger or over-aligned allocations, and allocations on
//     threads that never enter a scope (e.g. std::async validation
//     workers, and - deliberately - every doctest test binary, which never
//     constructs a ScopedCompileArena at all), always fall through to the
//     ordinary system allocator - this is the safe default for anything
//     outside an explicit, process-lifetime CLI scope.
//   - The arena is never reset. Free lists still recycle same-thread frees
//     within that one process-lifetime scope (so a single compile's peak
//     memory tracks its live working set, not its cumulative allocation
//     volume), but nothing is ever handed back to a *different* logical
//     "generation" the way a reset-per-compile design would. This is what
//     makes it safe: it is exactly as safe as "never free until process
//     exit," which was always the documented, accepted strategy for a
//     one-shot compile-then-exit CLI process.
//   - operator delete is always safe to call on any pointer, regardless of
//     which thread freed it or whether an arena is currently active on that
//     thread: every allocation carries a small header identifying how (and,
//     if applicable, by which arena) it was allocated, so delete never
//     guesses from ambient state.

namespace primec {

// RAII guard marking "the arena is active for the rest of this process."
// See the file comment above for the full contract - construct exactly one
// of these, near the top of main(), and let it live until process exit.
class ScopedCompileArena {
public:
  ScopedCompileArena();
  ~ScopedCompileArena();

  ScopedCompileArena(const ScopedCompileArena &) = delete;
  ScopedCompileArena &operator=(const ScopedCompileArena &) = delete;
  ScopedCompileArena(ScopedCompileArena &&) = delete;
  ScopedCompileArena &operator=(ScopedCompileArena &&) = delete;
};

} // namespace primec
