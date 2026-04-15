#include "third_party/doctest.h"

#include "primec/SemanticProduct.h"
#include "primec/SemanticsDefinitionPartitioner.h"
#include "primec/SemanticsDefinitionPrepass.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
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

} // namespace

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
  const bool parallelOk = semantics.validate(parallelProgram,
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

  const bool serialOk = semantics.validate(serialProgram,
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
  const bool parallelOk = semantics.validate(parallelProgram,
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

  const bool serialOk = semantics.validate(serialProgram,
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
  const bool parallelOk = semantics.validate(parallelProgram,
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

  const bool serialOk = semantics.validate(serialProgram,
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
  const bool parallelOk = semantics.validate(parallelProgram,
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
    formattedByWorkerCount[i] = primec::formatSemanticProgram(semanticProgram);
  }

  CHECK(formattedByWorkerCount[0] == formattedByWorkerCount[1]);
  CHECK(formattedByWorkerCount[1] == formattedByWorkerCount[2]);
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
