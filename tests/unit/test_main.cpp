#define DOCTEST_CONFIG_IMPLEMENT

#include "primec/testing/TestScratch.h"
#include "third_party/doctest.h"

// TODO-5233/TODO-5234: deliberately does NOT construct a
// primec::ScopedCompileArena anywhere in this binary. See
// docs/CompilerArenaAllocator.md and src/CompileArena.cpp's file comment:
// an earlier design gave every TEST_CASE its own reset compile scope, which
// reproducibly corrupted process-lifetime "magic static" values a few
// TEST_CASEs later. The arena that shipped is safe only because it is
// entered exactly once, at CLI startup, and never reset - which is exactly
// wrong for a binary that runs thousands of TEST_CASEs in one process, so
// the doctest binaries simply never enter a compile scope and stay on the
// system allocator, per TODO-5234's stop_rule fallback.
int main(int argc, char **argv) {
  primec::testing::ensureTestScratchEnvironment();

  doctest::Context context;
  context.applyCommandLine(argc, argv);
  return context.run();
}
