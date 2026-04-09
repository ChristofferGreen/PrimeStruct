#include "third_party/doctest.h"

#include "primec/SemanticProduct.h"
#include "primec/Semantics.h"

#include <array>
#include <cstddef>
#include <string>
#include <vector>

#include "test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.tsan_smoke");

namespace {

std::string buildParallelSuccessFixture(std::size_t leafCount) {
  std::string source;
  for (std::size_t i = 0; i < leafCount; ++i) {
    source += "[return<i32>]\n";
    source += "leaf_" + std::to_string(i) + "([i32] value) {\n";
    source += "  return(plus(value, " + std::to_string(i + 1) + "i32))\n";
    source += "}\n\n";
  }

  source += "[return<i32>]\n";
  source += "main() {\n";
  source += "  [i32 mut] total{0i32}\n";
  for (std::size_t i = 0; i < leafCount; ++i) {
    source += "  assign(total, plus(total, leaf_" + std::to_string(i) + "(1i32)))\n";
  }
  source += "  return(total)\n";
  source += "}\n";
  return source;
}

std::string buildParallelDiagnosticFixture(std::size_t definitionCount) {
  std::string source;
  for (std::size_t i = 0; i < definitionCount; ++i) {
    source += "[return<void>]\n";
    source += "broken_" + std::to_string(i) + "() {\n";
    source += "  missing_" + std::to_string(i) + "()\n";
    source += "}\n\n";
  }
  source += "[return<i32>]\n";
  source += "main() {\n";
  source += "  return(0i32)\n";
  source += "}\n";
  return source;
}

std::vector<std::string> extractDiagnosticMessages(
    const primec::SemanticDiagnosticInfo &diagnostics) {
  std::vector<std::string> messages;
  messages.reserve(diagnostics.records.size());
  for (const auto &record : diagnostics.records) {
    messages.push_back(record.message);
  }
  return messages;
}

} // namespace

TEST_CASE("thread sanitizer smoke keeps multithread semantic-product deterministic") {
  constexpr std::size_t leafCount = 24;
  const std::string source = buildParallelSuccessFixture(leafCount);
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  primec::Semantics semantics;

  std::array<std::string, 3> formattedOutputs;
  constexpr std::array<uint32_t, 3> workerCounts = {1u, 4u, 4u};

  for (std::size_t i = 0; i < workerCounts.size(); ++i) {
    primec::Program program = parseProgram(source);
    std::string error;
    primec::SemanticProgram semanticProgram;
    const bool ok = semantics.validate(program,
                                       "/main",
                                       error,
                                       defaults,
                                       defaults,
                                       {},
                                       nullptr,
                                       false,
                                       &semanticProgram,
                                       nullptr,
                                       workerCounts[i]);
    CHECK(ok);
    CHECK(error.empty());
    formattedOutputs[i] = primec::formatSemanticProgram(semanticProgram);
  }

  CHECK(formattedOutputs[0] == formattedOutputs[1]);
  CHECK(formattedOutputs[1] == formattedOutputs[2]);
}

TEST_CASE("thread sanitizer smoke keeps multithread diagnostic order deterministic") {
  constexpr std::size_t definitionCount = 24;
  const std::string source = buildParallelDiagnosticFixture(definitionCount);
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  primec::Semantics semantics;

  std::array<std::vector<std::string>, 3> diagnosticMessagesByRun;
  constexpr std::array<uint32_t, 3> workerCounts = {1u, 4u, 4u};

  for (std::size_t i = 0; i < workerCounts.size(); ++i) {
    primec::Program program = parseProgram(source);
    std::string error;
    primec::SemanticDiagnosticInfo diagnostics;
    const bool ok = semantics.validate(program,
                                       "/main",
                                       error,
                                       defaults,
                                       defaults,
                                       {},
                                       &diagnostics,
                                       true,
                                       nullptr,
                                       nullptr,
                                       workerCounts[i]);
    CHECK_FALSE(ok);
    CHECK_FALSE(error.empty());
    diagnosticMessagesByRun[i] = extractDiagnosticMessages(diagnostics);
  }

  CHECK(diagnosticMessagesByRun[0] == diagnosticMessagesByRun[1]);
  CHECK(diagnosticMessagesByRun[1] == diagnosticMessagesByRun[2]);
}

TEST_SUITE_END();
