#define DOCTEST_CONFIG_IMPLEMENT

#include "primec/testing/TestScratch.h"
#include "third_party/doctest.h"

int main(int argc, char **argv) {
  primec::testing::ensureTestScratchEnvironment();

  doctest::Context context;
  context.applyCommandLine(argc, argv);
  return context.run();
}
