#define DOCTEST_CONFIG_IMPLEMENT

#include "primec/testing/TestScratch.h"
#include "third_party/doctest.h"

// TODO-5233/TODO-5234/TODO-5235: deliberately does NOT construct a
// primec::ScopedCompileArena anywhere in this binary. See
// docs/CompilerArenaAllocator.md for the full history:
//
//   - TODO-5234 first tried giving every TEST_CASE its own reset compile
//     scope and reproducibly corrupted process-lifetime "magic static"
//     values a few TEST_CASEs later, falling back to never entering a
//     compile scope in this binary at all.
//   - TODO-5235 built a general escape hatch for that (SystemHeapScope /
//     systemHeapValue / registerArenaResetCallback in
//     primec/CompileArena.h) and re-attempted the same per-TEST_CASE reset
//     wiring under it. Each round of "fix the magic statics the crash
//     reproduction found, rebuild, rerun the full suite" turned up a
//     *different* magic static than the last, and - critically - each one
//     lived in a different, previously-unscoped corner of src/ (first
//     src/semantics, then a thread_local cache's own bucket-array buffer
//     surviving .clear(), then src/TransformRegistry.cpp, with
//     src/IrBackends.cpp/src/TempPaths.cpp/include/primec/ir/SoaPathHelpers.h
//     found by grep but not yet crash-confirmed at the point this was
//     stopped). That is exactly the exhaustiveness risk TODO-5234's own
//     writeup already flagged ("no practical way to enumerate every static
//     that might get lazily constructed inside a compile scope, forever, as
//     the codebase evolves") - now demonstrated empirically rather than
//     just reasoned about. Per TODO-5235's own stop_rule, resets were left
//     off here rather than shipped on the strength of "found and fixed
//     every instance I happened to hit so far." The SystemHeapScope
//     mechanism and every magic static already found and fixed remain in
//     place (harmless whether or not resets are ever turned on here), so a
//     future attempt starts with a smaller remaining surface, but this
//     binary still stays entirely on the system allocator, exactly as
//     TODO-5234 shipped it.
int main(int argc, char **argv) {
  primec::testing::ensureTestScratchEnvironment();

  doctest::Context context;
  context.applyCommandLine(argc, argv);
  return context.run();
}
