#include "third_party/doctest.h"

#include "primec/SemanticProduct.h"
#include "primec/SemanticValidationResult.h"
#include "primec/SemanticsBenchmark.h"
#include "primec/SemanticsDefinitionPartitioner.h"
#include "primec/SemanticsDefinitionPrepass.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.definition_partitioner");

namespace {

std::vector<std::size_t> flattenStableIndices(
    const primec::semantics::DefinitionPrepassSnapshot &snapshot,
    const std::vector<primec::semantics::DefinitionPartitionChunk> &partitions) {
  std::vector<std::size_t> indices;
  for (const auto &partition : partitions) {
    const std::size_t end =
        partition.stableOrderOffset + partition.stableOrderCount;
    for (std::size_t i = partition.stableOrderOffset; i < end; ++i) {
      indices.push_back(snapshot.declarationsInStableOrder[i].stableIndex);
    }
  }
  return indices;
}

std::vector<std::string> diagnosticMessages(
    const primec::SemanticDiagnosticInfo &diagnostics) {
  std::vector<std::string> messages;
  messages.reserve(diagnostics.records.size());
  for (const auto &record : diagnostics.records) {
    messages.push_back(record.message);
  }
  return messages;
}

bool anyMessageContains(const std::vector<std::string> &messages,
                        std::string_view needle) {
  return std::any_of(messages.begin(),
                     messages.end(),
                     [&](const std::string &message) {
                       return message.find(needle) != std::string::npos;
                     });
}

std::string readRepoText(std::filesystem::path relativePath) {
  const std::filesystem::path cwd = std::filesystem::current_path();
  const std::array<std::filesystem::path, 2> candidates = {
      cwd / relativePath,
      cwd.parent_path() / relativePath,
  };
  for (const auto &candidate : candidates) {
    std::ifstream file(candidate);
    if (!file) {
      continue;
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
  }
  ADD_FAIL("failed to read repo file: " << relativePath.string());
  return {};
}

bool validateWithWorkerCount(primec::Semantics &semantics,
                             primec::Program &program,
                             const std::string &entryPath,
                             std::string &error,
                             const std::vector<std::string> &defaultEffects,
                             const std::vector<std::string> &entryDefaultEffects,
                             const std::vector<std::string> &semanticTransforms = {},
                             primec::SemanticDiagnosticInfo *diagnosticInfo = nullptr,
                             bool collectDiagnostics = false,
                             primec::SemanticProgram *semanticProgramOut = nullptr,
                             const primec::SemanticProductBuildConfig *semanticProductBuildConfig = nullptr,
                             uint32_t workerCount = 1) {
  (void)semantics;
  primec::SemanticValidationBenchmarkConfig benchmarkConfig;
  benchmarkConfig.definitionValidationWorkerCount = workerCount;
  return primec::validateSemanticsForBenchmark(program,
                                               entryPath,
                                               error,
                                               defaultEffects,
                                               entryDefaultEffects,
                                               semanticTransforms,
                                               diagnosticInfo,
                                               collectDiagnostics,
                                               semanticProgramOut,
                                               semanticProductBuildConfig,
                                               benchmarkConfig);
}

} // namespace

TEST_CASE("semantic validation result sink preserves first-error adapter") {
  std::string legacyError;
  primec::SemanticDiagnosticInfo diagnostics;
  primec::SemanticValidationResultSink sink(legacyError, &diagnostics, true);
  sink.reset();

  sink.capturePrimarySpanIfUnset(3, 5);
  sink.addRelatedSpan(2, 1, "definition: /main");
  CHECK_FALSE(sink.fail("first fatal"));

  CHECK(legacyError == "first fatal");
  REQUIRE(diagnostics.records.size() == 1);
  CHECK(diagnostics.message == "first fatal");
  CHECK(diagnostics.records[0].message == "first fatal");
  CHECK(diagnostics.records[0].hasPrimarySpan);
  CHECK(diagnostics.records[0].primarySpan.line == 3);
  REQUIRE(diagnostics.records[0].relatedSpans.size() == 1);
  CHECK(diagnostics.records[0].relatedSpans[0].label == "definition: /main");

  legacyError.clear();
  std::vector<primec::SemanticDiagnosticRecord> records;
  primec::SemanticDiagnosticRecord second;
  second.message = "second fatal";
  records.push_back(second);
  primec::SemanticDiagnosticRecord third;
  third.message = "third fatal";
  records.push_back(third);

  CHECK_FALSE(sink.finalizeCollectedDiagnostics(records));
  CHECK(legacyError == "second fatal");
  REQUIRE(diagnostics.records.size() == 2);
  CHECK(diagnostics.records[0].message == "second fatal");
  CHECK(diagnostics.records[1].message == "third fatal");
}

TEST_CASE("definition partitioner emits deterministic balanced contiguous chunks") {
  const std::string source = R"(
[return<void>]
a() {}

[return<void>]
b() {}

[return<void>]
c() {}

[return<void>]
d() {}

[return<void>]
e() {}

[return<void>]
f() {}

[return<void>]
g() {}
)";

  const primec::Program program = parseProgram(source);
  const primec::semantics::DefinitionPrepassSnapshot snapshot =
      primec::semantics::buildDefinitionPrepassSnapshot(program);
  const std::vector<primec::semantics::DefinitionPartitionChunk> partitions =
      primec::semantics::partitionDefinitionsDeterministic(snapshot, 3);

  REQUIRE(partitions.size() == 3);
  CHECK(partitions[0].partitionKey == 0);
  CHECK(partitions[1].partitionKey == 1);
  CHECK(partitions[2].partitionKey == 2);
  CHECK(partitions[0].stableOrderOffset == 0);
  CHECK(partitions[0].stableOrderCount == 3);
  CHECK(partitions[1].stableOrderOffset == 3);
  CHECK(partitions[1].stableOrderCount == 2);
  CHECK(partitions[2].stableOrderOffset == 5);
  CHECK(partitions[2].stableOrderCount == 2);
}

TEST_CASE("definition partitioner handles zero and oversized partition counts deterministically") {
  const std::string source = R"(
[return<void>]
left() {}

[return<void>]
right() {}
)";

  const primec::Program program = parseProgram(source);
  const primec::semantics::DefinitionPrepassSnapshot snapshot =
      primec::semantics::buildDefinitionPrepassSnapshot(program);

  const std::vector<primec::semantics::DefinitionPartitionChunk> zeroPartitions =
      primec::semantics::partitionDefinitionsDeterministic(snapshot, 0);
  CHECK(zeroPartitions.empty());

  const std::vector<primec::semantics::DefinitionPartitionChunk> oversizedPartitions =
      primec::semantics::partitionDefinitionsDeterministic(snapshot, 4);
  REQUIRE(oversizedPartitions.size() == 4);
  CHECK(oversizedPartitions[0].stableOrderOffset == 0);
  CHECK(oversizedPartitions[0].stableOrderCount == 1);
  CHECK(oversizedPartitions[1].stableOrderOffset == 1);
  CHECK(oversizedPartitions[1].stableOrderCount == 1);
  CHECK(oversizedPartitions[2].stableOrderOffset == 2);
  CHECK(oversizedPartitions[2].stableOrderCount == 0);
  CHECK(oversizedPartitions[3].stableOrderOffset == 2);
  CHECK(oversizedPartitions[3].stableOrderCount == 0);
}

TEST_CASE("definition partitioner repeat runs are stable and cover each declaration once") {
  const std::string source = R"(
[return<void>]
alpha() {}

[return<void>]
beta() {}

[return<void>]
alpha() {}

[return<void>]
gamma() {}

[return<void>]
delta() {}
)";

  const primec::Program program = parseProgram(source);
  const primec::semantics::DefinitionPrepassSnapshot snapshot =
      primec::semantics::buildDefinitionPrepassSnapshot(program);

  const std::vector<primec::semantics::DefinitionPartitionChunk> firstPartitions =
      primec::semantics::partitionDefinitionsDeterministic(snapshot, 3);
  const std::vector<primec::semantics::DefinitionPartitionChunk> secondPartitions =
      primec::semantics::partitionDefinitionsDeterministic(snapshot, 3);
  REQUIRE(firstPartitions.size() == secondPartitions.size());
  for (std::size_t i = 0; i < firstPartitions.size(); ++i) {
    CHECK(firstPartitions[i].partitionKey == secondPartitions[i].partitionKey);
    CHECK(firstPartitions[i].stableOrderOffset ==
          secondPartitions[i].stableOrderOffset);
    CHECK(firstPartitions[i].stableOrderCount ==
          secondPartitions[i].stableOrderCount);
  }

  std::vector<std::size_t> flattened =
      flattenStableIndices(snapshot, firstPartitions);
  std::sort(flattened.begin(), flattened.end());
  CHECK(flattened == std::vector<std::size_t>{0, 1, 2, 3, 4});
}

TEST_CASE("two-chunk definition validation flag preserves success parity") {
  const std::string source = R"(
[return<i32>]
inc([i32] value) {
  return(value)
}

[return<i32>]
double_value([i32] value) {
  return(value)
}

[return<i32>]
main() {
  return(double_value(inc(4i32)))
}
)";

  const std::vector<std::string> defaults = {"io_out", "io_err"};
  primec::Program serialProgram = parseProgram(source);
  primec::Program parallelProgram = parseProgram(source);
  primec::Semantics semantics;
  std::string serialError;
  std::string parallelError;

  const bool serialOk =
      semantics.validate(serialProgram, "/main", serialError, defaults, defaults);
  const bool parallelOk = validateWithWorkerCount(semantics,
                                                  parallelProgram,
                                                  "/main",
                                                  parallelError,
                                                  defaults,
                                                  defaults,
                                                  {},
                                                  nullptr,
                                                  false,
                                                  nullptr,
                                                  nullptr,
                                                  2);

  CHECK(serialOk);
  CHECK(parallelOk);
  CHECK(serialError.empty());
  CHECK(parallelError.empty());
}

TEST_CASE("benchmark semantic entrypoint preserves production semantic-product output") {
  const std::string source = R"(
[return<i32>]
leaf([i32] value) {
  return(plus(value, 1i32))
}

[return<i32>]
main() {
  return(leaf(4i32))
}
)";

  const std::vector<std::string> defaults = {"io_out", "io_err"};
  primec::Program productionProgram = parseProgram(source);
  primec::Program benchmarkProgram = parseProgram(source);
  primec::Semantics semantics;
  primec::SemanticProgram productionSemanticProgram;
  primec::SemanticProgram benchmarkSemanticProgram;
  std::string productionError;
  std::string benchmarkError;

  const bool productionOk = semantics.validate(productionProgram,
                                               "/main",
                                               productionError,
                                               defaults,
                                               defaults,
                                               {},
                                               nullptr,
                                               false,
                                               &productionSemanticProgram);
  const bool benchmarkOk = validateWithWorkerCount(semantics,
                                                   benchmarkProgram,
                                                   "/main",
                                                   benchmarkError,
                                                   defaults,
                                                   defaults,
                                                   {},
                                                   nullptr,
                                                   false,
                                                   &benchmarkSemanticProgram,
                                                   nullptr,
                                                   1);

  CHECK(productionOk);
  CHECK(benchmarkOk);
  CHECK(productionError.empty());
  CHECK(benchmarkError.empty());
  CHECK(primec::formatSemanticProgram(productionSemanticProgram) ==
        primec::formatSemanticProgram(benchmarkSemanticProgram));
}

TEST_CASE("semantic validation plan keeps diagnostics stable with publication requested") {
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  primec::Semantics semantics;

  const std::string successSource = R"(
[return<i32>]
main() {
  return(1i32)
}
)";
  primec::Program successWithoutProduct = parseProgram(successSource);
  primec::Program successWithProduct = parseProgram(successSource);
  primec::SemanticDiagnosticInfo successWithoutDiagnostics;
  primec::SemanticDiagnosticInfo successWithDiagnostics;
  primec::SemanticProgram successSemanticProgram;
  std::string successWithoutError;
  std::string successWithError;

  const bool successWithoutOk = semantics.validate(successWithoutProduct,
                                                   "/main",
                                                   successWithoutError,
                                                   defaults,
                                                   defaults,
                                                   {},
                                                   &successWithoutDiagnostics,
                                                   true);
  const bool successWithOk = semantics.validate(successWithProduct,
                                                "/main",
                                                successWithError,
                                                defaults,
                                                defaults,
                                                {},
                                                &successWithDiagnostics,
                                                true,
                                                &successSemanticProgram);

  CHECK(successWithoutOk);
  CHECK(successWithOk);
  CHECK(successWithoutError == successWithError);
  CHECK(diagnosticMessages(successWithoutDiagnostics) ==
        diagnosticMessages(successWithDiagnostics));

  const std::string failureSource = R"(
[return<void>]
main() {
  missing_call()
}
)";
  primec::Program failureWithoutProduct = parseProgram(failureSource);
  primec::Program failureWithProduct = parseProgram(failureSource);
  primec::SemanticDiagnosticInfo failureWithoutDiagnostics;
  primec::SemanticDiagnosticInfo failureWithDiagnostics;
  primec::SemanticProgram failureSemanticProgram;
  std::string failureWithoutError;
  std::string failureWithError;

  const bool failureWithoutOk = semantics.validate(failureWithoutProduct,
                                                   "/main",
                                                   failureWithoutError,
                                                   defaults,
                                                   defaults,
                                                   {},
                                                   &failureWithoutDiagnostics,
                                                   true);
  const bool failureWithOk = semantics.validate(failureWithProduct,
                                                "/main",
                                                failureWithError,
                                                defaults,
                                                defaults,
                                                {},
                                                &failureWithDiagnostics,
                                                true,
                                                &failureSemanticProgram);

  CHECK_FALSE(failureWithoutOk);
  CHECK_FALSE(failureWithOk);
  CHECK(failureWithoutError == failureWithError);
  CHECK(diagnosticMessages(failureWithoutDiagnostics) ==
        diagnosticMessages(failureWithDiagnostics));
}

TEST_CASE("two-chunk definition validation flag preserves diagnostic ordering parity") {
  const std::string source = R"(
[return<void>]
alpha() {
  missing_alpha()
}

[return<void>]
beta() {
}

[return<void>]
gamma() {
  missing_gamma()
}

[return<void>]
main() {
}
)";

  const std::vector<std::string> defaults = {"io_out", "io_err"};
  primec::Program serialProgram = parseProgram(source);
  primec::Program parallelProgram = parseProgram(source);
  primec::Semantics semantics;
  std::string serialError;
  std::string parallelError;
  primec::SemanticDiagnosticInfo serialDiagnostics;
  primec::SemanticDiagnosticInfo parallelDiagnostics;

  const bool serialOk = validateWithWorkerCount(semantics,
                                                serialProgram,
                                                "/main",
                                                serialError,
                                                defaults,
                                                defaults,
                                                {},
                                                &serialDiagnostics,
                                                true,
                                                nullptr,
                                                nullptr,
                                                1);
  const bool parallelOk = validateWithWorkerCount(semantics,
                                                  parallelProgram,
                                                  "/main",
                                                  parallelError,
                                                  defaults,
                                                  defaults,
                                                  {},
                                                  &parallelDiagnostics,
                                                  true,
                                                  nullptr,
                                                  nullptr,
                                                  2);

  CHECK_FALSE(serialOk);
  CHECK_FALSE(parallelOk);
  CHECK_FALSE(serialError.empty());
  CHECK_FALSE(parallelError.empty());
  CHECK(diagnosticMessages(serialDiagnostics) ==
        diagnosticMessages(parallelDiagnostics));
}

TEST_CASE("n-chunk definition validation preserves diagnostic ordering parity") {
  const std::string source = R"(
[return<void>]
alpha() {
  missing_alpha()
}

[return<void>]
beta() {
}

[return<void>]
gamma() {
  missing_gamma()
}

[return<void>]
delta() {
  missing_delta()
}

[return<void>]
main() {
}
)";

  const std::vector<std::string> defaults = {"io_out", "io_err"};
  primec::Program serialProgram = parseProgram(source);
  primec::Program parallelProgram = parseProgram(source);
  primec::Semantics semantics;
  std::string serialError;
  std::string parallelError;
  primec::SemanticDiagnosticInfo serialDiagnostics;
  primec::SemanticDiagnosticInfo parallelDiagnostics;

  const bool serialOk = validateWithWorkerCount(semantics,
                                                serialProgram,
                                                "/main",
                                                serialError,
                                                defaults,
                                                defaults,
                                                {},
                                                &serialDiagnostics,
                                                true,
                                                nullptr,
                                                nullptr,
                                                1);
  const bool parallelOk = validateWithWorkerCount(semantics,
                                                  parallelProgram,
                                                  "/main",
                                                  parallelError,
                                                  defaults,
                                                  defaults,
                                                  {},
                                                  &parallelDiagnostics,
                                                  true,
                                                  nullptr,
                                                  nullptr,
                                                  4);

  CHECK_FALSE(serialOk);
  CHECK_FALSE(parallelOk);
  CHECK_FALSE(serialError.empty());
  CHECK_FALSE(parallelError.empty());
  CHECK(diagnosticMessages(serialDiagnostics) ==
        diagnosticMessages(parallelDiagnostics));
}

TEST_CASE("n-chunk definition validation diagnostics stay ordered across repeated runs") {
  const std::string source = R"(
[return<void>]
alpha() {
  missing_alpha()
}

[return<void>]
beta() {
}

[return<void>]
gamma() {
  missing_gamma()
}

[return<void>]
delta() {
  missing_delta()
}

[return<void>]
main() {
}
)";

  const std::vector<std::string> defaults = {"io_out", "io_err"};
  primec::Semantics semantics;
  std::vector<std::string> firstMessages;
  std::vector<std::string> secondMessages;

  for (int run = 0; run < 2; ++run) {
    primec::Program program = parseProgram(source);
    std::string error;
    primec::SemanticDiagnosticInfo diagnostics;
    const bool ok = validateWithWorkerCount(semantics,
                                            program,
                                            "/main",
                                            error,
                                            defaults,
                                            defaults,
                                            {},
                                            &diagnostics,
                                            true,
                                            nullptr,
                                            nullptr,
                                            4);
    CHECK_FALSE(ok);
    CHECK_FALSE(error.empty());
    if (run == 0) {
      firstMessages = diagnosticMessages(diagnostics);
    } else {
      secondMessages = diagnosticMessages(diagnostics);
    }
  }

  CHECK(firstMessages == secondMessages);
}

TEST_CASE("n-chunk semantic product output order stays stable across repeated runs") {
  const std::string source = R"(
[return<i32>]
leaf_a() {
  return(1i32)
}

[return<i32>]
leaf_b() {
  return(2i32)
}

[return<i32>]
sum_pair([i32] left, [i32] right) {
  return(left)
}

[return<i32>]
main() {
  [i32] a{leaf_a()}
  [i32] b{leaf_b()}
  [i32] c{sum_pair(a, b)}
  return(c)
}
)";

  const std::vector<std::string> defaults = {"io_out", "io_err"};
  primec::Semantics semantics;
  std::string firstFormatted;
  std::string secondFormatted;

  for (int run = 0; run < 2; ++run) {
    primec::Program program = parseProgram(source);
    std::string error;
    primec::SemanticProgram semanticProgram;
    const bool ok = validateWithWorkerCount(semantics,
                                            program,
                                            "/main",
                                            error,
                                            defaults,
                                            defaults,
                                            {},
                                            nullptr,
                                            false,
                                            &semanticProgram,
                                            nullptr,
                                            4);
    CHECK(ok);
    CHECK(error.empty());
    const std::string formatted = primec::formatSemanticProgram(semanticProgram);
    CHECK(formatted.find("/leaf_a") != std::string::npos);
    CHECK(formatted.find("/leaf_b") != std::string::npos);
    CHECK(formatted.find("/sum_pair") != std::string::npos);
    if (run == 0) {
      firstFormatted = formatted;
    } else {
      secondFormatted = formatted;
    }
  }

  CHECK(firstFormatted == secondFormatted);
}

TEST_CASE("single-thread and n-chunk semantic product outputs are equivalent") {
  const std::string source = R"(
[return<i32>]
leaf_a() {
  return(1i32)
}

[return<i32>]
leaf_b() {
  return(2i32)
}

[return<i32>]
sum_pair([i32] left, [i32] right) {
  return(left)
}

[return<i32>]
main() {
  [i32] a{leaf_a()}
  [i32] b{leaf_b()}
  [i32] c{sum_pair(a, b)}
  return(c)
}
)";

  const std::vector<std::string> defaults = {"io_out", "io_err"};
  primec::Semantics semantics;
  primec::Program serialProgram = parseProgram(source);
  primec::Program parallelProgram = parseProgram(source);
  std::string serialError;
  std::string parallelError;
  primec::SemanticProgram serialSemanticProduct;
  primec::SemanticProgram parallelSemanticProduct;

  const bool serialOk = validateWithWorkerCount(semantics,
                                                serialProgram,
                                                "/main",
                                                serialError,
                                                defaults,
                                                defaults,
                                                {},
                                                nullptr,
                                                false,
                                                &serialSemanticProduct,
                                                nullptr,
                                                1);
  const bool parallelOk = validateWithWorkerCount(semantics,
                                                  parallelProgram,
                                                  "/main",
                                                  parallelError,
                                                  defaults,
                                                  defaults,
                                                  {},
                                                  nullptr,
                                                  false,
                                                  &parallelSemanticProduct,
                                                  nullptr,
                                                  4);
  CHECK(serialOk);
  CHECK(parallelOk);
  CHECK(serialError.empty());
  CHECK(parallelError.empty());
  CHECK(primec::formatSemanticProgram(serialSemanticProduct) ==
        primec::formatSemanticProgram(parallelSemanticProduct));
}

TEST_CASE("semantic memory inline fixture semantic product is equivalent across worker counts") {
  const std::string source =
      readRepoText("benchmarks/semantic_memory/fixtures/inline_math_body.prime");
  REQUIRE(source.find("poly3") != std::string::npos);

  const std::vector<std::string> defaults = {"io_out", "io_err"};
  primec::Semantics semantics;
  std::array<std::string, 3> formattedByWorkerCount;
  std::array<std::size_t, 3> directCallCounts{};
  std::array<std::size_t, 3> queryFactCounts{};
  constexpr std::array<uint32_t, 3> workerCounts = {1u, 2u, 4u};

  for (std::size_t i = 0; i < workerCounts.size(); ++i) {
    primec::Program program = parseProgram(source);
    std::string error;
    primec::SemanticProgram semanticProgram;
    const bool ok = validateWithWorkerCount(semantics,
                                            program,
                                            "/bench/main",
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
    directCallCounts[i] = semanticProgram.directCallTargets.size();
    queryFactCounts[i] = semanticProgram.queryFacts.size();
    formattedByWorkerCount[i] = primec::formatSemanticProgram(semanticProgram);
  }

  CHECK(directCallCounts[0] == directCallCounts[1]);
  CHECK(directCallCounts[1] == directCallCounts[2]);
  CHECK(queryFactCounts[0] == queryFactCounts[1]);
  CHECK(queryFactCounts[1] == queryFactCounts[2]);
  CHECK(formattedByWorkerCount[0] == formattedByWorkerCount[1]);
  CHECK(formattedByWorkerCount[1] == formattedByWorkerCount[2]);
}

TEST_CASE("definition validation diagnostics are equivalent across worker counts 1,2,4") {
  const std::string source = R"(
[return<void>]
alpha() {
  missing_alpha()
}

[return<void>]
beta() {
}

[return<void>]
gamma() {
  missing_gamma()
}

[return<void>]
delta() {
  missing_delta()
}

[return<void>]
main() {
}
)";

  const std::vector<std::string> defaults = {"io_out", "io_err"};
  primec::Semantics semantics;
  std::array<std::vector<std::string>, 3> diagnosticMessagesByWorkerCount;
  constexpr std::array<uint32_t, 3> workerCounts = {1u, 2u, 4u};

  for (std::size_t i = 0; i < workerCounts.size(); ++i) {
    primec::Program program = parseProgram(source);
    std::string error;
    primec::SemanticDiagnosticInfo diagnostics;
    const bool ok = validateWithWorkerCount(semantics,
                                            program,
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
    diagnosticMessagesByWorkerCount[i] = diagnosticMessages(diagnostics);
  }

  CHECK(diagnosticMessagesByWorkerCount[0] == diagnosticMessagesByWorkerCount[1]);
  CHECK(diagnosticMessagesByWorkerCount[1] == diagnosticMessagesByWorkerCount[2]);
}

TEST_CASE("semantic product output is equivalent across worker counts 1,2,4") {
  const std::string source = R"(
[return<i32>]
leaf_a() {
  return(1i32)
}

[return<i32>]
leaf_b() {
  return(2i32)
}

[return<i32>]
sum_pair([i32] left, [i32] right) {
  return(left)
}

[return<i32>]
main() {
  [i32] a{leaf_a()}
  [i32] b{leaf_b()}
  [i32] c{sum_pair(a, b)}
  return(c)
}
)";

  const std::vector<std::string> defaults = {"io_out", "io_err"};
  primec::Semantics semantics;
  std::array<std::string, 3> formattedByWorkerCount;
  constexpr std::array<uint32_t, 3> workerCounts = {1u, 2u, 4u};

  for (std::size_t i = 0; i < workerCounts.size(); ++i) {
    primec::Program program = parseProgram(source);
    std::string error;
    primec::SemanticProgram semanticProgram;
    const bool ok = validateWithWorkerCount(semantics,
                                            program,
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
    formattedByWorkerCount[i] = primec::formatSemanticProgram(semanticProgram);
  }

  CHECK(formattedByWorkerCount[0] == formattedByWorkerCount[1]);
  CHECK(formattedByWorkerCount[1] == formattedByWorkerCount[2]);
}

TEST_CASE("worker-local definition validation context keeps semantic product equivalent across worker counts 1,2,4") {
  const std::string source = R"(
[return<Result<i32, i32>>]
add_with_default([i32] value, [i32] delta{2i32}) {
  [i32] current{plus(value, delta)}
  return(Result.ok(current))
}

[return<i32>]
record_error([i32] err, [i32] offset) {
  return(plus(err, offset))
}

[return<i32> on_error<i32, /record_error>(7i32)]
main() {
  [auto] current{add_with_default(4i32)}
  [i32] value{try(current)}
  return(plus(value, 1i32))
}
)";

  const std::vector<std::string> defaults = {"io_out", "io_err"};
  primec::Semantics semantics;
  std::array<std::string, 3> formattedByWorkerCount;
  std::array<std::size_t, 3> onErrorFactCounts{};
  struct OnErrorSnapshot {
    bool present = false;
    std::string definitionPath;
    std::string returnKind;
    std::string handlerPath;
    std::string errorType;
    std::size_t boundArgCount = 0;
    std::vector<std::string> boundArgTexts;
    bool returnResultHasValue = false;
    std::string returnResultValueType;
    std::string returnResultErrorType;
    uint64_t semanticNodeId = 0;
  };
  std::array<OnErrorSnapshot, 3> onErrorSnapshots{};
  constexpr std::array<uint32_t, 3> workerCounts = {1u, 2u, 4u};

  for (std::size_t i = 0; i < workerCounts.size(); ++i) {
    primec::Program program = parseProgram(source);
    std::string error;
    primec::SemanticProgram semanticProgram;
    const bool ok = validateWithWorkerCount(semantics,
                                            program,
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
    CHECK(semanticProgram.callableSummaries.size() >= 3);
    onErrorFactCounts[i] = semanticProgram.onErrorFacts.size();
    formattedByWorkerCount[i] = primec::formatSemanticProgram(semanticProgram);
    CHECK(formattedByWorkerCount[i].find("/add_with_default") != std::string::npos);
    CHECK(formattedByWorkerCount[i].find("/record_error") != std::string::npos);
    const auto onErrorFacts =
        primec::semanticProgramOnErrorFactView(semanticProgram);
    REQUIRE(onErrorFacts.size() == 1);
    const auto &entry = *onErrorFacts.front();
    onErrorSnapshots[i].present = true;
    onErrorSnapshots[i].definitionPath = std::string(
        primec::semanticProgramOnErrorFactDefinitionPath(semanticProgram,
                                                         entry));
    onErrorSnapshots[i].returnKind = entry.returnKind;
    onErrorSnapshots[i].handlerPath = std::string(
        primec::semanticProgramOnErrorFactHandlerPath(semanticProgram, entry));
    onErrorSnapshots[i].errorType = entry.errorType;
    onErrorSnapshots[i].boundArgCount = entry.boundArgCount;
    onErrorSnapshots[i].boundArgTexts = entry.boundArgTexts;
    onErrorSnapshots[i].returnResultHasValue = entry.returnResultHasValue;
    onErrorSnapshots[i].returnResultValueType = entry.returnResultValueType;
    onErrorSnapshots[i].returnResultErrorType = entry.returnResultErrorType;
    onErrorSnapshots[i].semanticNodeId = entry.semanticNodeId;
  }

  CHECK(onErrorFactCounts[0] == 1);
  CHECK(onErrorFactCounts[0] == onErrorFactCounts[1]);
  CHECK(onErrorFactCounts[1] == onErrorFactCounts[2]);
  CHECK(onErrorSnapshots[0].present);
  CHECK(onErrorSnapshots[1].present);
  CHECK(onErrorSnapshots[2].present);
  CHECK(onErrorSnapshots[0].definitionPath == onErrorSnapshots[1].definitionPath);
  CHECK(onErrorSnapshots[0].definitionPath == onErrorSnapshots[2].definitionPath);
  CHECK(onErrorSnapshots[0].returnKind == onErrorSnapshots[1].returnKind);
  CHECK(onErrorSnapshots[0].returnKind == onErrorSnapshots[2].returnKind);
  CHECK(onErrorSnapshots[0].handlerPath == onErrorSnapshots[1].handlerPath);
  CHECK(onErrorSnapshots[0].handlerPath == onErrorSnapshots[2].handlerPath);
  CHECK(onErrorSnapshots[0].errorType == onErrorSnapshots[1].errorType);
  CHECK(onErrorSnapshots[0].errorType == onErrorSnapshots[2].errorType);
  CHECK(onErrorSnapshots[0].boundArgCount == onErrorSnapshots[1].boundArgCount);
  CHECK(onErrorSnapshots[0].boundArgCount == onErrorSnapshots[2].boundArgCount);
  CHECK(onErrorSnapshots[0].boundArgTexts == onErrorSnapshots[1].boundArgTexts);
  CHECK(onErrorSnapshots[0].boundArgTexts == onErrorSnapshots[2].boundArgTexts);
  CHECK(onErrorSnapshots[0].returnResultHasValue ==
        onErrorSnapshots[1].returnResultHasValue);
  CHECK(onErrorSnapshots[0].returnResultHasValue ==
        onErrorSnapshots[2].returnResultHasValue);
  CHECK(onErrorSnapshots[0].returnResultValueType ==
        onErrorSnapshots[1].returnResultValueType);
  CHECK(onErrorSnapshots[0].returnResultValueType ==
        onErrorSnapshots[2].returnResultValueType);
  CHECK(onErrorSnapshots[0].returnResultErrorType ==
        onErrorSnapshots[1].returnResultErrorType);
  CHECK(onErrorSnapshots[0].returnResultErrorType ==
        onErrorSnapshots[2].returnResultErrorType);
  CHECK(onErrorSnapshots[0].semanticNodeId == onErrorSnapshots[1].semanticNodeId);
  CHECK(onErrorSnapshots[0].semanticNodeId == onErrorSnapshots[2].semanticNodeId);
  CHECK(onErrorSnapshots[0].definitionPath == "/main");
  CHECK(onErrorSnapshots[0].returnKind == "value");
  CHECK(onErrorSnapshots[0].handlerPath == "/record_error");
  CHECK(onErrorSnapshots[0].errorType == "i32");
  CHECK(onErrorSnapshots[0].boundArgCount == 1);
  CHECK(onErrorSnapshots[0].boundArgTexts == std::vector<std::string>{"7i32"});
  CHECK_FALSE(onErrorSnapshots[0].returnResultHasValue);
  CHECK(onErrorSnapshots[0].returnResultValueType.empty());
  CHECK(onErrorSnapshots[0].returnResultErrorType.empty());
  CHECK(onErrorSnapshots[0].semanticNodeId > 0);
  CHECK(formattedByWorkerCount[0] == formattedByWorkerCount[1]);
  CHECK(formattedByWorkerCount[1] == formattedByWorkerCount[2]);
}

TEST_CASE("worker-local definition validation context keeps diagnostics equivalent across worker counts 1,2,4") {
  const std::string source = R"(
[return<Result<i32, i32>>]
broken_default([i32] value, [i32] delta{missing_default()}) {
  [i32] current{plus(value, 1i32)}
  return(Result.ok(current))
}

[return<i32>]
record_error([i32] err, [i32] offset) {
  return(plus(err, offset))
}

[return<i32> on_error<i32, /record_error>(missing_offset())]
broken_on_error() {
  [auto] current{Result.ok(4i32)}
  [i32] value{try(current)}
  return(value)
}

[return<i32>]
broken_body([i32] value) {
  [i32] next{plus(value, 1i32)}
  missing_body(next)
  return(next)
}

[return<i32>]
main() {
  return(0i32)
}
)";

  const std::vector<std::string> defaults = {"io_out", "io_err"};
  primec::Semantics semantics;
  std::array<std::vector<std::string>, 3> messagesByWorkerCount;
  constexpr std::array<uint32_t, 3> workerCounts = {1u, 2u, 4u};

  for (std::size_t i = 0; i < workerCounts.size(); ++i) {
    primec::Program program = parseProgram(source);
    std::string error;
    primec::SemanticDiagnosticInfo diagnostics;
    const bool ok = validateWithWorkerCount(semantics,
                                            program,
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
    messagesByWorkerCount[i] = diagnosticMessages(diagnostics);
    CHECK(messagesByWorkerCount[i].size() >= 3);
    CHECK(anyMessageContains(messagesByWorkerCount[i], "missing_default"));
    CHECK(anyMessageContains(messagesByWorkerCount[i], "missing_offset"));
    CHECK(anyMessageContains(messagesByWorkerCount[i], "missing_body"));
  }

  CHECK(messagesByWorkerCount[0] == messagesByWorkerCount[1]);
  CHECK(messagesByWorkerCount[1] == messagesByWorkerCount[2]);
}

TEST_CASE("semantic product index families are equivalent across worker counts 1,2,4") {
  const std::filesystem::path tempPath = makeTempSemanticSourcePath();
  {
    std::ofstream file(tempPath);
    REQUIRE(file.good());
    file << R"(
[return<Result<int, i32>>]
id([i32] value) {
  return(Result.ok(value))
}

[return<i32>]
/vector/count([vector<i32>] self) {
  return(17i32)
}

[return<i32>]
unexpected_error([i32] err) {
  return(err)
}

[return<i32> effects(heap_alloc) on_error<i32, /unexpected_error>]
main() {
  [auto] direct{id(1i32)}
  [auto] values{vector<i32>(1i32)}
  [i32] method{/vector/count(values)}
  [i32] bridge{/vector/count(values)}
  [i32] tried{try(direct)}
  return(plus(plus(method, bridge), tried))
}
)";
  }

  struct IndexFamilySnapshot {
    std::string formattedSemanticProduct;
    std::size_t callableSummaryCount = 0;
    std::size_t directCallTargetCount = 0;
    std::size_t methodCallTargetCount = 0;
    std::size_t bridgePathChoiceCount = 0;
    std::size_t bindingFactCount = 0;
    std::size_t localAutoFactCount = 0;
    std::size_t queryFactCount = 0;
    std::size_t tryFactCount = 0;
    std::size_t onErrorFactCount = 0;
    std::size_t returnFactCount = 0;
  };

  const auto runWithWorkerCount = [&](uint32_t workerCount) {
    primec::Options options;
    options.inputPath = tempPath.string();
    options.entryPath = "/main";
    options.emitKind = "native";
    options.collectDiagnostics = true;
    options.benchmarkSemanticDefinitionValidationWorkerCount = workerCount;
    primec::addDefaultStdlibInclude(options.inputPath, options.importPaths);

    primec::CompilePipelineOutput output;
    primec::CompilePipelineDiagnosticInfo diagnosticInfo;
    primec::CompilePipelineErrorStage errorStage = primec::CompilePipelineErrorStage::None;
    std::string error;
    const bool ok = primec::runCompilePipeline(options, output, errorStage, error, &diagnosticInfo);

    CHECK(ok);
    CHECK(error.empty());
    CHECK(errorStage == primec::CompilePipelineErrorStage::None);
    CHECK_FALSE(output.hasFailure);
    REQUIRE(output.hasSemanticProgram);
    CHECK(diagnosticMessages(diagnosticInfo).empty());

    IndexFamilySnapshot snapshot;
    snapshot.formattedSemanticProduct = primec::formatSemanticProgram(output.semanticProgram);
    snapshot.callableSummaryCount = output.semanticProgram.callableSummaries.size();
    snapshot.directCallTargetCount = output.semanticProgram.directCallTargets.size();
    snapshot.methodCallTargetCount = output.semanticProgram.methodCallTargets.size();
    snapshot.bridgePathChoiceCount = output.semanticProgram.bridgePathChoices.size();
    snapshot.bindingFactCount = output.semanticProgram.bindingFacts.size();
    snapshot.localAutoFactCount = output.semanticProgram.localAutoFacts.size();
    snapshot.queryFactCount = output.semanticProgram.queryFacts.size();
    snapshot.tryFactCount = output.semanticProgram.tryFacts.size();
    snapshot.onErrorFactCount = output.semanticProgram.onErrorFacts.size();
    snapshot.returnFactCount = output.semanticProgram.returnFacts.size();
    return snapshot;
  };

  const IndexFamilySnapshot singleWorker = runWithWorkerCount(1u);
  const IndexFamilySnapshot twoWorkers = runWithWorkerCount(2u);
  const IndexFamilySnapshot fourWorkers = runWithWorkerCount(4u);

  std::error_code ec;
  std::filesystem::remove(tempPath, ec);

  CHECK(singleWorker.formattedSemanticProduct == twoWorkers.formattedSemanticProduct);
  CHECK(singleWorker.formattedSemanticProduct == fourWorkers.formattedSemanticProduct);
  CHECK(twoWorkers.formattedSemanticProduct == fourWorkers.formattedSemanticProduct);

  CHECK(singleWorker.callableSummaryCount == twoWorkers.callableSummaryCount);
  CHECK(singleWorker.callableSummaryCount == fourWorkers.callableSummaryCount);
  CHECK(singleWorker.directCallTargetCount == twoWorkers.directCallTargetCount);
  CHECK(singleWorker.directCallTargetCount == fourWorkers.directCallTargetCount);
  CHECK(singleWorker.methodCallTargetCount == twoWorkers.methodCallTargetCount);
  CHECK(singleWorker.methodCallTargetCount == fourWorkers.methodCallTargetCount);
  CHECK(singleWorker.bridgePathChoiceCount == twoWorkers.bridgePathChoiceCount);
  CHECK(singleWorker.bridgePathChoiceCount == fourWorkers.bridgePathChoiceCount);
  CHECK(singleWorker.bindingFactCount == twoWorkers.bindingFactCount);
  CHECK(singleWorker.bindingFactCount == fourWorkers.bindingFactCount);
  CHECK(singleWorker.localAutoFactCount == twoWorkers.localAutoFactCount);
  CHECK(singleWorker.localAutoFactCount == fourWorkers.localAutoFactCount);
  CHECK(singleWorker.queryFactCount == twoWorkers.queryFactCount);
  CHECK(singleWorker.queryFactCount == fourWorkers.queryFactCount);
  CHECK(singleWorker.tryFactCount == twoWorkers.tryFactCount);
  CHECK(singleWorker.tryFactCount == fourWorkers.tryFactCount);
  CHECK(singleWorker.onErrorFactCount == twoWorkers.onErrorFactCount);
  CHECK(singleWorker.onErrorFactCount == fourWorkers.onErrorFactCount);
  CHECK(singleWorker.returnFactCount == twoWorkers.returnFactCount);
  CHECK(singleWorker.returnFactCount == fourWorkers.returnFactCount);

  CHECK(singleWorker.callableSummaryCount > 0);
  CHECK(singleWorker.directCallTargetCount > 0);
  CHECK(singleWorker.methodCallTargetCount > 0);
  CHECK(singleWorker.bridgePathChoiceCount == 0);
  CHECK(singleWorker.bindingFactCount > 0);
  CHECK(singleWorker.localAutoFactCount > 0);
  CHECK(singleWorker.queryFactCount > 0);
  CHECK(singleWorker.tryFactCount > 0);
  CHECK(singleWorker.onErrorFactCount > 0);
  CHECK(singleWorker.returnFactCount > 0);
}

TEST_SUITE_END();
