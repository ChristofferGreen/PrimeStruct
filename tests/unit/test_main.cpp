#define DOCTEST_CONFIG_IMPLEMENT

#include <optional>

#include "primec/support/CompileArena.h"
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
//
// PRIMEC_TEST_ARENA_RESET_PER_CASE (default undefined - normal builds are
// unaffected): a still-experimental, manually-opted-in build-time switch
// used only for the TODO-5235 poison-audit investigation
// (docs/CompilerArenaAllocator.md). When defined, this binary constructs
// one primec::ScopedCompileArena per doctest TEST_CASE via the listener
// below, re-enabling the exact reset-per-TEST_CASE design described above.
// Do NOT define this for any build whose result is meant to be trusted
// without first confirming, for that exact build, that the full suite
// passes clean under PRIMESTRUCT_ARENA_POISON_AUDIT.
#if defined(PRIMEC_TEST_ARENA_RESET_PER_CASE)
namespace {

class ArenaResetPerTestCaseListener : public doctest::IReporter {
 public:
  explicit ArenaResetPerTestCaseListener(const doctest::ContextOptions &) {}

  void report_query(const doctest::QueryData &) override {}
  void test_run_start() override {}
  void test_run_end(const doctest::TestRunStats &) override {}

  void test_case_start(const doctest::TestCaseData &) override {
    scope_.emplace();
  }
  void test_case_reenter(const doctest::TestCaseData &) override {}
  void test_case_end(const doctest::CurrentTestCaseStats &) override {
    scope_.reset();
  }
  void test_case_exception(const doctest::TestCaseException &) override {}
  void subcase_start(const doctest::SubcaseSignature &) override {}
  void subcase_end() override {}
  void log_assert(const doctest::AssertData &) override {}
  void log_message(const doctest::MessageData &) override {}
  void test_case_skipped(const doctest::TestCaseData &) override {}

 private:
  // In-place slot (no extra allocation of its own beyond the optional's
  // inline storage) so construction/destruction order exactly mirrors
  // ScopedCompileArena's RAII contract: constructed at test_case_start,
  // destroyed at test_case_end (which is also where all of a TEST_CASE's
  // SUBCASE re-enters have finished, per doctest's own IReporter contract).
  std::optional<primec::ScopedCompileArena> scope_;
};

REGISTER_LISTENER("arena_reset_per_test_case", 1, ArenaResetPerTestCaseListener);

}  // namespace
#endif

int main(int argc, char **argv) {
  primec::testing::ensureTestScratchEnvironment();

  doctest::Context context;
  context.applyCommandLine(argc, argv);
  return context.run();
}
