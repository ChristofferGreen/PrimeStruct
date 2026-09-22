#define DOCTEST_CONFIG_IMPLEMENT

#include <optional>

#include "primec/support/CompileArena.h"
#include "primec/testing/TestScratch.h"
#include "third_party/doctest.h"

// TODO-5233/TODO-5234/TODO-5235: this file is shared by every doctest
// binary in the project, but whether it constructs a
// primec::ScopedCompileArena per TEST_CASE is decided PER BINARY, via
// whether PRIMEC_TEST_ARENA_RESET_PER_CASE is defined for that specific
// target (see CMakeLists.txt's target_compile_definitions calls). See
// docs/CompilerArenaAllocator.md for the full history:
//
//   - TODO-5234 first tried giving every TEST_CASE its own reset compile
//     scope and reproducibly corrupted process-lifetime "magic static"
//     values a few TEST_CASEs later, falling back to never entering a
//     compile scope in any test binary at all.
//   - TODO-5235 built a general escape hatch for that (SystemHeapScope /
//     systemHeapValue / registerArenaResetCallback in
//     primec/CompileArena.h) and re-attempted the same per-TEST_CASE reset
//     wiring under it. Many rounds of "fix the magic statics/thread_local
//     hazards a poison-audit crash found, rebuild, rerun the full suite"
//     each turned up a genuinely different hazard than the last (see the
//     doc's full round-by-round history), including hazards inside the
//     vendored third_party/doctest.h itself and, in a 2026-09-22 round, an
//     unrelated ODR-violation stack-buffer-overflow the audit tooling
//     surfaced (fixed separately, mirror-guarded by
//     scripts/check_testing_mirror_structs.py).
//   - As of 2026-09-22, PrimeStruct_backend_ir_tests and
//     PrimeStruct_semantics_tests - the two long-lived binaries this task
//     was scoped to from the start - have each passed TWO independent,
//     full PRIMESTRUCT_ARENA_POISON_AUDIT sweeps (all shards clean, zero
//     hazards) and now ship with PRIMEC_TEST_ARENA_RESET_PER_CASE on by
//     default (via their own target_compile_definitions in
//     CMakeLists.txt), so TODO-5234's original reset-per-scope design is
//     actually live for those two binaries. Every OTHER test binary
//     (PrimeStruct_backend_runtime_tests, PrimeStruct_compile_run_tests,
//     PrimeStruct_parser_tests, PrimeStruct_text_filter_tests,
//     PrimeStruct_misc_tests, PrimeStruct_compile_time_tests, ...) has
//     never been poison-audited and must NOT be switched on without first
//     running scripts/run_arena_poison_audit.sh against it clean, twice.
//
// PRIMEC_TEST_ARENA_RESET_PER_CASE: when defined for a given target
// (unconditionally for PrimeStruct_backend_ir_tests/PrimeStruct_semantics_tests,
// or globally via the PRIMESTRUCT_TEST_ARENA_RESET_PER_CASE/
// PRIMESTRUCT_ARENA_POISON_AUDIT CMake options for a manual investigation
// build), this binary constructs one primec::ScopedCompileArena per
// doctest TEST_CASE via the listener below. Do NOT define this for any
// OTHER binary's build whose result is meant to be trusted without first
// confirming, for that exact build, that the full suite passes clean under
// PRIMESTRUCT_ARENA_POISON_AUDIT.
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
