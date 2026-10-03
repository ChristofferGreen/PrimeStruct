#include "third_party/doctest.h"

#include "test_ir_pipeline_backends_registry_shared.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.backends.registry");

TEST_CASE("compile pipeline benchmark semantic allocation counters are opt-in and populated") {
  const std::filesystem::path tempPath = makeTempIrPipelineSourcePath();
  {
    std::ofstream file(tempPath);
    REQUIRE(file.good());
    file << R"(namespace bench {
[return<i32>]
callee([i32] value) {
  return(value)
}
[return<i32>]
main() {
  [i32 mut] value{4i32}
  return(callee(value))
}
}
)";
  }

  primec::Options options;
  options.inputPath = tempPath.string();
  options.entryPath = "/bench/main";
  options.emitKind = "native";
  options.dumpStage = "semantic-product";
  options.benchmarkSemanticAllocationCounters = true;
  primec::addDefaultStdlibInclude(options.inputPath, options.importPaths);

  primec::CompilePipelineOutput output;
  primec::CompilePipelineErrorStage errorStage = primec::CompilePipelineErrorStage::None;
  std::string error;
  const bool ok = primec::runCompilePipeline(options, output, errorStage, error);

  std::error_code ec;
  std::filesystem::remove(tempPath, ec);

  REQUIRE(ok);
  CHECK(error.empty());
  REQUIRE(output.hasSemanticPhaseCounters);
  CHECK(output.semanticPhaseCounters.validation.allocationCount > 0);
  CHECK(output.semanticPhaseCounters.validation.allocatedBytes > 0);
  CHECK(output.semanticPhaseCounters.semanticProductBuild.allocationCount > 0);
}

TEST_CASE("compile pipeline benchmark semantic rss checkpoints are opt-in and populated") {
  const std::filesystem::path tempPath = makeTempIrPipelineSourcePath();
  {
    std::ofstream file(tempPath);
    REQUIRE(file.good());
    file << R"(namespace bench {
[return<i32>]
callee([i32] value) {
  return(value)
}
[return<i32>]
main() {
  [i32 mut] value{4i32}
  return(callee(value))
}
}
)";
  }

  primec::Options options;
  options.inputPath = tempPath.string();
  options.entryPath = "/bench/main";
  options.emitKind = "native";
  options.dumpStage = "semantic-product";
  options.benchmarkSemanticRssCheckpoints = true;
  primec::addDefaultStdlibInclude(options.inputPath, options.importPaths);

  primec::CompilePipelineOutput output;
  primec::CompilePipelineErrorStage errorStage = primec::CompilePipelineErrorStage::None;
  std::string error;
  const bool ok = primec::runCompilePipeline(options, output, errorStage, error);

  std::error_code ec;
  std::filesystem::remove(tempPath, ec);

  REQUIRE(ok);
  CHECK(error.empty());
  REQUIRE(output.hasSemanticPhaseCounters);
  checkOptionalRssCheckpointSnapshot(output.semanticPhaseCounters.validation);
  checkOptionalRssCheckpointSnapshot(output.semanticPhaseCounters.semanticProductBuild);
}

TEST_CASE("compile pipeline benchmark fact-family allowlist keeps only selected collectors") {
  const std::filesystem::path tempPath = makeTempIrPipelineSourcePath();
  {
    std::ofstream file(tempPath);
    REQUIRE(file.good());
    file << R"(namespace bench {
[void]
callee() {}
[return<i32>]
main() {
  callee()
  return(0i32)
}
}
)";
  }

  primec::Options options;
  options.inputPath = tempPath.string();
  options.entryPath = "/bench/main";
  options.emitKind = "native";
  options.dumpStage = "semantic-product";
  options.benchmarkSemanticFactFamiliesSpecified = true;
  options.benchmarkSemanticFactFamilies = {"callable_summaries"};
  primec::addDefaultStdlibInclude(options.inputPath, options.importPaths);

  primec::CompilePipelineOutput output;
  primec::CompilePipelineErrorStage errorStage = primec::CompilePipelineErrorStage::None;
  std::string error;
  const bool ok = primec::runCompilePipeline(options, output, errorStage, error);

  std::error_code ec;
  std::filesystem::remove(tempPath, ec);

  REQUIRE(ok);
  REQUIRE(output.hasSemanticProgram);
  CHECK_FALSE(output.semanticProgram.callableSummaries.empty());
  CHECK(output.semanticProgram.directCallTargets.empty());
  CHECK(output.semanticProgram.bindingFacts.empty());
  CHECK(output.semanticProgram.returnFacts.empty());
  CHECK(output.semanticProgram.queryFacts.empty());
}

TEST_CASE("compile pipeline direct and bridge collector merge keeps output-order parity") {
  const std::filesystem::path tempPath = makeTempIrPipelineSourcePath();
  {
    std::ofstream file(tempPath);
    REQUIRE(file.good());
    file << R"(namespace bench {
[void]
callee() {}
[void]
main() {
  [vector<i32>] values{vector<i32>()}
  callee()
  [i32] countA{count(values)}
  [i32] countB{count(values)}
  [i32] capA{capacity(values)}
}
}
)";
  }

  const auto runWithFamilies = [&](const std::vector<std::string> &families) {
    primec::Options options;
    options.inputPath = tempPath.string();
    options.entryPath = "/bench/main";
    options.emitKind = "native";
    options.dumpStage = "semantic-product";
    options.benchmarkSemanticFactFamiliesSpecified = true;
    options.benchmarkSemanticFactFamilies = families;
    primec::addDefaultStdlibInclude(options.inputPath, options.importPaths);

    primec::CompilePipelineOutput output;
    primec::CompilePipelineErrorStage errorStage = primec::CompilePipelineErrorStage::None;
    std::string error;
    const bool ok = primec::runCompilePipeline(options, output, errorStage, error);
    REQUIRE(ok);
    CHECK(error.empty());
    REQUIRE(output.hasSemanticProgram);
    return output;
  };

  const primec::CompilePipelineOutput combinedOutput =
      runWithFamilies({"direct_call_targets", "bridge_path_choices"});
  const primec::CompilePipelineOutput directOnlyOutput =
      runWithFamilies({"direct_call_targets"});
  const primec::CompilePipelineOutput bridgeOnlyOutput =
      runWithFamilies({"bridge_path_choices"});

  std::error_code ec;
  std::filesystem::remove(tempPath, ec);

  const auto combinedDirectTargets =
      primec::semanticProgramDirectCallTargetView(combinedOutput.semanticProgram);
  const auto directOnlyTargets =
      primec::semanticProgramDirectCallTargetView(directOnlyOutput.semanticProgram);
  REQUIRE(combinedDirectTargets.size() == directOnlyTargets.size());
  for (std::size_t i = 0; i < combinedDirectTargets.size(); ++i) {
    const auto *combinedEntry = combinedDirectTargets[i];
    const auto *directOnlyEntry = directOnlyTargets[i];
    REQUIRE(combinedEntry != nullptr);
    REQUIRE(directOnlyEntry != nullptr);
    CHECK(combinedEntry->scopePath == directOnlyEntry->scopePath);
    CHECK(combinedEntry->callName == directOnlyEntry->callName);
    CHECK(primec::semanticProgramDirectCallTargetResolvedPath(combinedOutput.semanticProgram, *combinedEntry) ==
          primec::semanticProgramDirectCallTargetResolvedPath(directOnlyOutput.semanticProgram, *directOnlyEntry));
  }

  const auto combinedBridgeChoices =
      primec::semanticProgramBridgePathChoiceView(combinedOutput.semanticProgram);
  const auto bridgeOnlyChoices =
      primec::semanticProgramBridgePathChoiceView(bridgeOnlyOutput.semanticProgram);
  REQUIRE(combinedBridgeChoices.size() == bridgeOnlyChoices.size());
  for (std::size_t i = 0; i < combinedBridgeChoices.size(); ++i) {
    const auto *combinedEntry = combinedBridgeChoices[i];
    const auto *bridgeOnlyEntry = bridgeOnlyChoices[i];
    REQUIRE(combinedEntry != nullptr);
    REQUIRE(bridgeOnlyEntry != nullptr);
    CHECK(combinedEntry->scopePath == bridgeOnlyEntry->scopePath);
    CHECK(combinedEntry->collectionFamily == bridgeOnlyEntry->collectionFamily);
    CHECK(primec::semanticProgramBridgePathChoiceHelperName(combinedOutput.semanticProgram, *combinedEntry) ==
          primec::semanticProgramBridgePathChoiceHelperName(bridgeOnlyOutput.semanticProgram, *bridgeOnlyEntry));
    CHECK(primec::semanticProgramResolveCallTargetString(combinedOutput.semanticProgram, combinedEntry->chosenPathId) ==
          primec::semanticProgramResolveCallTargetString(bridgeOnlyOutput.semanticProgram,
                                                         bridgeOnlyEntry->chosenPathId));
  }
}

TEST_CASE("compile pipeline benchmark worker-count stress keeps /std/math/* semantic-product dumps deterministic") {
  constexpr std::size_t localDefinitionCount = 64;
  const std::filesystem::path tempPath = makeTempIrPipelineSourcePath();
  {
    std::ofstream file(tempPath);
    REQUIRE(file.good());
    file << buildMathStressSemanticSource(localDefinitionCount);
  }

  struct StressSnapshot {
    std::string dumpOutput;
    std::size_t definitionCount = 0;
    std::size_t callableSummaryCount = 0;
    std::size_t directCallTargetCount = 0;
  };

  const auto runStress = [&]() {
    primec::Options options;
    options.inputPath = tempPath.string();
    options.entryPath = "/main";
    options.emitKind = "native";
    options.dumpStage = "semantic-product";
    options.collectDiagnostics = true;
    options.benchmarkSemanticDefinitionValidationWorkerCount = 4;
    primec::addDefaultStdlibInclude(options.inputPath, options.importPaths);

    primec::CompilePipelineOutput output;
    primec::CompilePipelineDiagnosticInfo diagnosticInfo;
    primec::CompilePipelineErrorStage errorStage = primec::CompilePipelineErrorStage::None;
    std::string error;
    const bool ok = primec::runCompilePipeline(options, output, errorStage, error, &diagnosticInfo);

    REQUIRE(ok);
    CHECK(error.empty());
    CHECK(errorStage == primec::CompilePipelineErrorStage::None);
    CHECK_FALSE(output.hasFailure);
    CHECK(output.semanticProductRequested);
    CHECK(output.semanticProductBuilt);
    REQUIRE(output.hasSemanticProgram);
    REQUIRE(output.hasDumpOutput);
    CHECK(std::find(output.program.imports.begin(),
                    output.program.imports.end(),
                    "/std/math/*") != output.program.imports.end());
    CHECK(output.program.definitions.size() >= localDefinitionCount + 1);
    CHECK(output.semanticProgram.callableSummaries.size() >= localDefinitionCount + 1);
    CHECK(output.semanticProgram.directCallTargets.size() >= localDefinitionCount);

    StressSnapshot snapshot;
    snapshot.dumpOutput = output.dumpOutput;
    snapshot.definitionCount = output.program.definitions.size();
    snapshot.callableSummaryCount = output.semanticProgram.callableSummaries.size();
    snapshot.directCallTargetCount = output.semanticProgram.directCallTargets.size();
    return snapshot;
  };

  const StressSnapshot firstRun = runStress();
  const StressSnapshot secondRun = runStress();

  std::error_code ec;
  std::filesystem::remove(tempPath, ec);

  CHECK(firstRun.dumpOutput == secondRun.dumpOutput);
  CHECK(firstRun.definitionCount == secondRun.definitionCount);
  CHECK(firstRun.callableSummaryCount == secondRun.callableSummaryCount);
  CHECK(firstRun.directCallTargetCount == secondRun.directCallTargetCount);
}

TEST_CASE("compile pipeline full semantic-product dump golden stays stable across 1,2,4 workers") {
  constexpr std::size_t localDefinitionCount = 64;
  const std::filesystem::path tempPath = makeTempIrPipelineSourcePath();
  {
    std::ofstream file(tempPath);
    REQUIRE(file.good());
    file << buildMathStressSemanticSource(localDefinitionCount);
  }

  struct StressSnapshot {
    std::string dumpOutput;
    std::size_t definitionCount = 0;
    std::size_t callableSummaryCount = 0;
    std::size_t directCallTargetCount = 0;
  };

  const auto runWithWorkerCount = [&](int workerCount) {
    primec::Options options;
    options.inputPath = tempPath.string();
    options.entryPath = "/main";
    options.emitKind = "native";
    options.dumpStage = "semantic-product";
    options.collectDiagnostics = true;
    options.benchmarkSemanticDefinitionValidationWorkerCount = workerCount;
    primec::addDefaultStdlibInclude(options.inputPath, options.importPaths);

    primec::CompilePipelineOutput output;
    primec::CompilePipelineDiagnosticInfo diagnosticInfo;
    primec::CompilePipelineErrorStage errorStage = primec::CompilePipelineErrorStage::None;
    std::string error;
    const bool ok = primec::runCompilePipeline(options, output, errorStage, error, &diagnosticInfo);

    REQUIRE(ok);
    CHECK(error.empty());
    CHECK(errorStage == primec::CompilePipelineErrorStage::None);
    CHECK_FALSE(output.hasFailure);
    CHECK(output.semanticProductRequested);
    CHECK(output.semanticProductBuilt);
    REQUIRE(output.hasSemanticProgram);
    REQUIRE(output.hasDumpOutput);
    CHECK(std::find(output.program.imports.begin(),
                    output.program.imports.end(),
                    "/std/math/*") != output.program.imports.end());
    CHECK(output.program.definitions.size() >= localDefinitionCount + 1);
    CHECK(output.semanticProgram.callableSummaries.size() >= localDefinitionCount + 1);
    CHECK(output.semanticProgram.directCallTargets.size() >= localDefinitionCount);

    StressSnapshot snapshot;
    snapshot.dumpOutput = output.dumpOutput;
    snapshot.definitionCount = output.program.definitions.size();
    snapshot.callableSummaryCount = output.semanticProgram.callableSummaries.size();
    snapshot.directCallTargetCount = output.semanticProgram.directCallTargets.size();
    return snapshot;
  };

  const StressSnapshot singleWorker = runWithWorkerCount(1);
  const StressSnapshot twoWorkers = runWithWorkerCount(2);
  const StressSnapshot fourWorkers = runWithWorkerCount(4);

  std::error_code ec;
  std::filesystem::remove(tempPath, ec);

  CHECK_FALSE(singleWorker.dumpOutput.empty());
  CHECK(singleWorker.dumpOutput.find("semantic_product {") != std::string::npos);
  CHECK(singleWorker.dumpOutput.find("/std/math/abs") != std::string::npos);
  CHECK(singleWorker.dumpOutput.find("/main") != std::string::npos);
  const std::string singleTwoDiff = describeSemanticProductDumpMismatch(
      "1-worker golden", "2-worker", singleWorker.dumpOutput, twoWorkers.dumpOutput);
  const std::string singleFourDiff = describeSemanticProductDumpMismatch(
      "1-worker golden", "4-worker", singleWorker.dumpOutput, fourWorkers.dumpOutput);
  const std::string twoFourDiff = describeSemanticProductDumpMismatch(
      "2-worker", "4-worker", twoWorkers.dumpOutput, fourWorkers.dumpOutput);
  CHECK_MESSAGE(singleTwoDiff.empty(), singleTwoDiff);
  CHECK_MESSAGE(singleFourDiff.empty(), singleFourDiff);
  CHECK_MESSAGE(twoFourDiff.empty(), twoFourDiff);
  CHECK(singleWorker.definitionCount == twoWorkers.definitionCount);
  CHECK(singleWorker.definitionCount == fourWorkers.definitionCount);
  CHECK(twoWorkers.definitionCount == fourWorkers.definitionCount);
  CHECK(singleWorker.callableSummaryCount == twoWorkers.callableSummaryCount);
  CHECK(singleWorker.callableSummaryCount == fourWorkers.callableSummaryCount);
  CHECK(twoWorkers.callableSummaryCount == fourWorkers.callableSummaryCount);
  CHECK(singleWorker.directCallTargetCount == twoWorkers.directCallTargetCount);
  CHECK(singleWorker.directCallTargetCount == fourWorkers.directCallTargetCount);
  CHECK(twoWorkers.directCallTargetCount == fourWorkers.directCallTargetCount);
}

TEST_CASE("compile pipeline benchmark worker-count stress keeps /std/math/* diagnostics deterministic") {
  constexpr std::size_t localDefinitionCount = 64;
  const std::filesystem::path tempPath = makeTempIrPipelineSourcePath();
  {
    std::ofstream file(tempPath);
    REQUIRE(file.good());
    file << buildMathStressDiagnosticSource(localDefinitionCount);
  }

  const auto runStress = [&]() {
    primec::Options options;
    options.inputPath = tempPath.string();
    options.entryPath = "/main";
    options.emitKind = "native";
    options.collectDiagnostics = true;
    options.benchmarkSemanticDefinitionValidationWorkerCount = 4;
    primec::addDefaultStdlibInclude(options.inputPath, options.importPaths);

    primec::CompilePipelineOutput output;
    primec::CompilePipelineDiagnosticInfo diagnosticInfo;
    primec::CompilePipelineErrorStage errorStage = primec::CompilePipelineErrorStage::None;
    std::string error;
    const bool ok = primec::runCompilePipeline(options, output, errorStage, error, &diagnosticInfo);

    CHECK_FALSE(ok);
    CHECK(errorStage == primec::CompilePipelineErrorStage::Semantic);
    REQUIRE(output.hasFailure);
    CHECK(output.failure.stage == primec::CompilePipelineErrorStage::Semantic);
    CHECK(output.failure.message == error);
    CHECK_FALSE(output.failure.diagnosticInfo.records.empty());
    CHECK_FALSE(diagnosticInfo.records.empty());
    return compileDiagnosticMessages(output.failure.diagnosticInfo);
  };

  const std::vector<std::string> firstRunMessages = runStress();
  const std::vector<std::string> secondRunMessages = runStress();

  std::error_code ec;
  std::filesystem::remove(tempPath, ec);

  CHECK(firstRunMessages == secondRunMessages);
  CHECK(firstRunMessages.size() >= 2);
}

TEST_CASE("compile pipeline benchmark worker-count equivalence keeps /std/math/* diagnostics stable across 1,2,4 workers") {
  constexpr std::size_t localDefinitionCount = 64;
  const std::filesystem::path tempPath = makeTempIrPipelineSourcePath();
  {
    std::ofstream file(tempPath);
    REQUIRE(file.good());
    file << buildMathStressDiagnosticSource(localDefinitionCount);
  }

  const auto runWithWorkerCount = [&](int workerCount) {
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

    CHECK_FALSE(ok);
    CHECK(errorStage == primec::CompilePipelineErrorStage::Semantic);
    REQUIRE(output.hasFailure);
    CHECK(output.failure.stage == primec::CompilePipelineErrorStage::Semantic);
    CHECK(output.failure.message == error);
    CHECK_FALSE(output.failure.diagnosticInfo.records.empty());
    CHECK_FALSE(diagnosticInfo.records.empty());
    return compileDiagnosticMessages(output.failure.diagnosticInfo);
  };

  const std::vector<std::string> singleWorkerMessages = runWithWorkerCount(1);
  const std::vector<std::string> twoWorkerMessages = runWithWorkerCount(2);
  const std::vector<std::string> fourWorkerMessages = runWithWorkerCount(4);

  std::error_code ec;
  std::filesystem::remove(tempPath, ec);

  CHECK(singleWorkerMessages == twoWorkerMessages);
  CHECK(singleWorkerMessages == fourWorkerMessages);
  CHECK(twoWorkerMessages == fourWorkerMessages);
  CHECK(singleWorkerMessages.size() >= 2);
}

TEST_CASE("compile pipeline benchmark worker-count equivalence keeps semantic-product") {
  const std::filesystem::path tempPath = makeTempIrPipelineSourcePath();
  {
    std::ofstream file(tempPath);
    REQUIRE(file.good());
    file << R"(
[return<Result<int, i32>>]
id([i32] value) {
  return(Result.ok(value))
}

[return<i32>]
unexpected_error([i32] err) {
  return(err)
}

[return<i32> effects(heap_alloc) on_error<i32, /unexpected_error>]
main() {
  [auto] direct{id(1i32)}
  [i32] tried{try(direct)}
  return(tried)
}
)";
  }

  struct CallableSummarySnapshot {
    bool present = false;
    bool isExecution = false;
    std::string returnKind;
    bool isCompute = false;
    bool isUnsafe = false;
    std::vector<std::string> activeEffects;
    bool hasResultType = false;
    bool resultTypeHasValue = false;
    std::string resultValueType;
    std::string resultErrorType;
    bool hasOnError = false;
    std::string onErrorHandlerPath;
    std::string onErrorErrorType;
    std::size_t onErrorBoundArgCount = 0;
    uint64_t semanticNodeId = 0;
  };

  struct OnErrorFactSnapshot {
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

  struct IndexFamilySnapshot {
    std::string formattedSemanticProduct;
    std::vector<std::string> callTargetStringTable;
    std::size_t directCallTargetCount = 0;
    std::size_t methodCallTargetCount = 0;
    std::size_t bridgePathChoiceCount = 0;
    std::size_t bindingFactCount = 0;
    std::size_t localAutoFactCount = 0;
    std::size_t queryFactCount = 0;
    std::size_t tryFactCount = 0;
    std::size_t onErrorFactCount = 0;
    std::size_t returnFactCount = 0;
    CallableSummarySnapshot idCallableSummary;
    CallableSummarySnapshot mainCallableSummary;
    OnErrorFactSnapshot mainOnErrorFact;
  };

  const auto captureCallableSummary =
      [](const primec::SemanticProgram &semanticProgram, std::string_view fullPath) {
        CallableSummarySnapshot snapshot;
        const auto *entry =
            primec::semanticProgramLookupPublishedCallableSummary(semanticProgram, fullPath);
        if (entry == nullptr) {
          return snapshot;
        }

        snapshot.present = true;
        snapshot.isExecution = entry->isExecution;
        snapshot.returnKind = entry->returnKind;
        snapshot.isCompute = entry->isCompute;
        snapshot.isUnsafe = entry->isUnsafe;
        snapshot.activeEffects = entry->activeEffects;
        snapshot.hasResultType = entry->hasResultType;
        snapshot.resultTypeHasValue = entry->resultTypeHasValue;
        snapshot.resultValueType = entry->resultValueType;
        snapshot.resultErrorType = entry->resultErrorType;
        snapshot.hasOnError = entry->hasOnError;
        snapshot.onErrorHandlerPath = entry->onErrorHandlerPath;
        snapshot.onErrorErrorType = entry->onErrorErrorType;
        snapshot.onErrorBoundArgCount = entry->onErrorBoundArgCount;
        snapshot.semanticNodeId = entry->semanticNodeId;
        return snapshot;
      };

  const auto runWithWorkerCount = [&](int workerCount) {
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

    REQUIRE(ok);
    CHECK(error.empty());
    CHECK(errorStage == primec::CompilePipelineErrorStage::None);
    CHECK_FALSE(output.hasFailure);
    REQUIRE(output.hasSemanticProgram);

    IndexFamilySnapshot snapshot;
    snapshot.formattedSemanticProduct = primec::formatSemanticProgram(output.semanticProgram);
    snapshot.callTargetStringTable = output.semanticProgram.callTargetStringTable;
    snapshot.directCallTargetCount = output.semanticProgram.directCallTargets.size();
    snapshot.methodCallTargetCount = output.semanticProgram.methodCallTargets.size();
    snapshot.bridgePathChoiceCount = output.semanticProgram.bridgePathChoices.size();
    snapshot.bindingFactCount = output.semanticProgram.bindingFacts.size();
    snapshot.localAutoFactCount = output.semanticProgram.localAutoFacts.size();
    snapshot.queryFactCount = output.semanticProgram.queryFacts.size();
    snapshot.tryFactCount = output.semanticProgram.tryFacts.size();
    snapshot.onErrorFactCount = output.semanticProgram.onErrorFacts.size();
    snapshot.returnFactCount = output.semanticProgram.returnFacts.size();
    snapshot.idCallableSummary =
        captureCallableSummary(output.semanticProgram, "/id");
    snapshot.mainCallableSummary =
        captureCallableSummary(output.semanticProgram, "/main");
    if (const auto *entry = findSemanticEntry(
            primec::semanticProgramOnErrorFactView(output.semanticProgram),
            [&output](const primec::SemanticProgramOnErrorFact &candidate) {
              return primec::semanticProgramOnErrorFactDefinitionPath(
                         output.semanticProgram, candidate) == "/main";
            });
        entry != nullptr) {
      snapshot.mainOnErrorFact.present = true;
      snapshot.mainOnErrorFact.definitionPath = std::string(
          primec::semanticProgramOnErrorFactDefinitionPath(
              output.semanticProgram, *entry));
      snapshot.mainOnErrorFact.returnKind = entry->returnKind;
      snapshot.mainOnErrorFact.handlerPath = std::string(
          primec::semanticProgramOnErrorFactHandlerPath(
              output.semanticProgram, *entry));
      snapshot.mainOnErrorFact.errorType = entry->errorType;
      snapshot.mainOnErrorFact.boundArgCount = entry->boundArgCount;
      snapshot.mainOnErrorFact.boundArgTexts = entry->boundArgTexts;
      snapshot.mainOnErrorFact.returnResultHasValue =
          entry->returnResultHasValue;
      snapshot.mainOnErrorFact.returnResultValueType =
          entry->returnResultValueType;
      snapshot.mainOnErrorFact.returnResultErrorType =
          entry->returnResultErrorType;
      snapshot.mainOnErrorFact.semanticNodeId = entry->semanticNodeId;
    }
    return snapshot;
  };

  const IndexFamilySnapshot singleWorker = runWithWorkerCount(1);
  const IndexFamilySnapshot twoWorkers = runWithWorkerCount(2);
  const IndexFamilySnapshot fourWorkers = runWithWorkerCount(4);

  std::error_code ec;
  std::filesystem::remove(tempPath, ec);

  CHECK_FALSE(singleWorker.formattedSemanticProduct.empty());
  CHECK_FALSE(twoWorkers.formattedSemanticProduct.empty());
  CHECK_FALSE(fourWorkers.formattedSemanticProduct.empty());
  CHECK(singleWorker.formattedSemanticProduct.find("/id") != std::string::npos);
  CHECK(singleWorker.formattedSemanticProduct.find("/main") != std::string::npos);
  CHECK(singleWorker.formattedSemanticProduct.find("/unexpected_error") != std::string::npos);
  CHECK(twoWorkers.formattedSemanticProduct.find("/id") != std::string::npos);
  CHECK(twoWorkers.formattedSemanticProduct.find("/main") != std::string::npos);
  CHECK(twoWorkers.formattedSemanticProduct.find("/unexpected_error") != std::string::npos);
  CHECK(fourWorkers.formattedSemanticProduct.find("/id") != std::string::npos);
  CHECK(fourWorkers.formattedSemanticProduct.find("/main") != std::string::npos);
  CHECK(fourWorkers.formattedSemanticProduct.find("/unexpected_error") != std::string::npos);
  CHECK_FALSE(singleWorker.callTargetStringTable.empty());
  CHECK_FALSE(twoWorkers.callTargetStringTable.empty());
  CHECK_FALSE(fourWorkers.callTargetStringTable.empty());

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

  CHECK(singleWorker.idCallableSummary.present);
  CHECK(twoWorkers.idCallableSummary.present);
  CHECK(fourWorkers.idCallableSummary.present);
  CHECK(singleWorker.idCallableSummary.isExecution == twoWorkers.idCallableSummary.isExecution);
  CHECK(singleWorker.idCallableSummary.isExecution == fourWorkers.idCallableSummary.isExecution);
  CHECK(singleWorker.idCallableSummary.returnKind == twoWorkers.idCallableSummary.returnKind);
  CHECK(singleWorker.idCallableSummary.returnKind == fourWorkers.idCallableSummary.returnKind);
  CHECK(singleWorker.idCallableSummary.hasResultType == twoWorkers.idCallableSummary.hasResultType);
  CHECK(singleWorker.idCallableSummary.hasResultType == fourWorkers.idCallableSummary.hasResultType);
  CHECK(singleWorker.idCallableSummary.resultTypeHasValue ==
        twoWorkers.idCallableSummary.resultTypeHasValue);
  CHECK(singleWorker.idCallableSummary.resultTypeHasValue ==
        fourWorkers.idCallableSummary.resultTypeHasValue);
  CHECK(singleWorker.idCallableSummary.resultValueType == twoWorkers.idCallableSummary.resultValueType);
  CHECK(singleWorker.idCallableSummary.resultValueType == fourWorkers.idCallableSummary.resultValueType);
  CHECK(singleWorker.idCallableSummary.resultErrorType == twoWorkers.idCallableSummary.resultErrorType);
  CHECK(singleWorker.idCallableSummary.resultErrorType == fourWorkers.idCallableSummary.resultErrorType);
  CHECK(singleWorker.idCallableSummary.semanticNodeId == twoWorkers.idCallableSummary.semanticNodeId);
  CHECK(singleWorker.idCallableSummary.semanticNodeId == fourWorkers.idCallableSummary.semanticNodeId);
  CHECK(singleWorker.idCallableSummary.semanticNodeId > 0);

  CHECK(singleWorker.mainCallableSummary.present);
  CHECK(twoWorkers.mainCallableSummary.present);
  CHECK(fourWorkers.mainCallableSummary.present);
  CHECK(singleWorker.mainCallableSummary.isExecution == twoWorkers.mainCallableSummary.isExecution);
  CHECK(singleWorker.mainCallableSummary.isExecution == fourWorkers.mainCallableSummary.isExecution);
  CHECK(singleWorker.mainCallableSummary.returnKind == twoWorkers.mainCallableSummary.returnKind);
  CHECK(singleWorker.mainCallableSummary.returnKind == fourWorkers.mainCallableSummary.returnKind);
  CHECK(singleWorker.mainCallableSummary.isCompute == twoWorkers.mainCallableSummary.isCompute);
  CHECK(singleWorker.mainCallableSummary.isCompute == fourWorkers.mainCallableSummary.isCompute);
  CHECK(singleWorker.mainCallableSummary.isUnsafe == twoWorkers.mainCallableSummary.isUnsafe);
  CHECK(singleWorker.mainCallableSummary.isUnsafe == fourWorkers.mainCallableSummary.isUnsafe);
  CHECK(singleWorker.mainCallableSummary.activeEffects == twoWorkers.mainCallableSummary.activeEffects);
  CHECK(singleWorker.mainCallableSummary.activeEffects == fourWorkers.mainCallableSummary.activeEffects);
  CHECK(singleWorker.mainCallableSummary.hasOnError == twoWorkers.mainCallableSummary.hasOnError);
  CHECK(singleWorker.mainCallableSummary.hasOnError == fourWorkers.mainCallableSummary.hasOnError);
  CHECK(singleWorker.mainCallableSummary.onErrorHandlerPath ==
        twoWorkers.mainCallableSummary.onErrorHandlerPath);
  CHECK(singleWorker.mainCallableSummary.onErrorHandlerPath ==
        fourWorkers.mainCallableSummary.onErrorHandlerPath);
  CHECK(singleWorker.mainCallableSummary.onErrorErrorType ==
        twoWorkers.mainCallableSummary.onErrorErrorType);
  CHECK(singleWorker.mainCallableSummary.onErrorErrorType ==
        fourWorkers.mainCallableSummary.onErrorErrorType);
  CHECK(singleWorker.mainCallableSummary.onErrorBoundArgCount ==
        twoWorkers.mainCallableSummary.onErrorBoundArgCount);
  CHECK(singleWorker.mainCallableSummary.onErrorBoundArgCount ==
        fourWorkers.mainCallableSummary.onErrorBoundArgCount);
  CHECK(singleWorker.mainCallableSummary.semanticNodeId ==
        twoWorkers.mainCallableSummary.semanticNodeId);
  CHECK(singleWorker.mainCallableSummary.semanticNodeId ==
        fourWorkers.mainCallableSummary.semanticNodeId);
  CHECK(singleWorker.mainCallableSummary.semanticNodeId > 0);

  CHECK(singleWorker.mainOnErrorFact.present);
  CHECK(twoWorkers.mainOnErrorFact.present);
  CHECK(fourWorkers.mainOnErrorFact.present);
  CHECK(singleWorker.mainOnErrorFact.definitionPath ==
        twoWorkers.mainOnErrorFact.definitionPath);
  CHECK(singleWorker.mainOnErrorFact.definitionPath ==
        fourWorkers.mainOnErrorFact.definitionPath);
  CHECK(singleWorker.mainOnErrorFact.returnKind ==
        twoWorkers.mainOnErrorFact.returnKind);
  CHECK(singleWorker.mainOnErrorFact.returnKind ==
        fourWorkers.mainOnErrorFact.returnKind);
  CHECK(singleWorker.mainOnErrorFact.handlerPath ==
        twoWorkers.mainOnErrorFact.handlerPath);
  CHECK(singleWorker.mainOnErrorFact.handlerPath ==
        fourWorkers.mainOnErrorFact.handlerPath);
  CHECK(singleWorker.mainOnErrorFact.errorType ==
        twoWorkers.mainOnErrorFact.errorType);
  CHECK(singleWorker.mainOnErrorFact.errorType ==
        fourWorkers.mainOnErrorFact.errorType);
  CHECK(singleWorker.mainOnErrorFact.boundArgCount ==
        twoWorkers.mainOnErrorFact.boundArgCount);
  CHECK(singleWorker.mainOnErrorFact.boundArgCount ==
        fourWorkers.mainOnErrorFact.boundArgCount);
  CHECK(singleWorker.mainOnErrorFact.boundArgTexts ==
        twoWorkers.mainOnErrorFact.boundArgTexts);
  CHECK(singleWorker.mainOnErrorFact.boundArgTexts ==
        fourWorkers.mainOnErrorFact.boundArgTexts);
  CHECK(singleWorker.mainOnErrorFact.returnResultHasValue ==
        twoWorkers.mainOnErrorFact.returnResultHasValue);
  CHECK(singleWorker.mainOnErrorFact.returnResultHasValue ==
        fourWorkers.mainOnErrorFact.returnResultHasValue);
  CHECK(singleWorker.mainOnErrorFact.returnResultValueType ==
        twoWorkers.mainOnErrorFact.returnResultValueType);
  CHECK(singleWorker.mainOnErrorFact.returnResultValueType ==
        fourWorkers.mainOnErrorFact.returnResultValueType);
  CHECK(singleWorker.mainOnErrorFact.returnResultErrorType ==
        twoWorkers.mainOnErrorFact.returnResultErrorType);
  CHECK(singleWorker.mainOnErrorFact.returnResultErrorType ==
        fourWorkers.mainOnErrorFact.returnResultErrorType);
  CHECK(singleWorker.directCallTargetCount > 0);
  CHECK(singleWorker.methodCallTargetCount > 0);
  CHECK(singleWorker.bridgePathChoiceCount == 0);
  CHECK(singleWorker.bindingFactCount > 0);
  CHECK(singleWorker.localAutoFactCount > 0);
  CHECK(singleWorker.queryFactCount > 0);
  CHECK(singleWorker.tryFactCount > 0);
  CHECK(singleWorker.onErrorFactCount > 0);
  CHECK(singleWorker.returnFactCount > 0);
  CHECK_FALSE(singleWorker.idCallableSummary.isExecution);
  CHECK(singleWorker.idCallableSummary.returnKind == "i64");
  CHECK(singleWorker.idCallableSummary.hasResultType);
  CHECK(singleWorker.idCallableSummary.resultTypeHasValue);
  CHECK(singleWorker.idCallableSummary.resultValueType == "int");
  CHECK(singleWorker.idCallableSummary.resultErrorType == "i32");
  CHECK_FALSE(singleWorker.mainCallableSummary.isExecution);
  CHECK(singleWorker.mainCallableSummary.returnKind == "i32");
  CHECK(singleWorker.mainCallableSummary.activeEffects ==
        std::vector<std::string>{"heap_alloc"});
  CHECK(singleWorker.mainCallableSummary.hasOnError);
  CHECK(singleWorker.mainCallableSummary.onErrorHandlerPath == "/unexpected_error");
  CHECK(singleWorker.mainCallableSummary.onErrorErrorType == "i32");
  CHECK(singleWorker.mainCallableSummary.onErrorBoundArgCount == 0);
  CHECK(singleWorker.mainOnErrorFact.definitionPath == "/main");
  CHECK(singleWorker.mainOnErrorFact.returnKind == "i32");
  CHECK(singleWorker.mainOnErrorFact.handlerPath == "/unexpected_error");
  CHECK(singleWorker.mainOnErrorFact.errorType == "i32");
  CHECK(singleWorker.mainOnErrorFact.boundArgCount == 0);
  CHECK(singleWorker.mainOnErrorFact.boundArgTexts.empty());
  CHECK_FALSE(singleWorker.mainOnErrorFact.returnResultHasValue);
  CHECK(singleWorker.mainOnErrorFact.returnResultValueType.empty());
  CHECK(singleWorker.mainOnErrorFact.returnResultErrorType.empty());
}

TEST_CASE("compile pipeline graph-local-auto benchmark shadows preserve published local-auto facts") {
  const std::filesystem::path tempPath = makeTempIrPipelineSourcePath();
  {
    std::ofstream file(tempPath);
    REQUIRE(file.good());
    file << R"(
import /std/file/*

[return<Result<i32, FileError>>]
lookup([i32] input) {
  return(Result.ok(plus(input, 1i32)))
}

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error(err.why())
}

[return<i32> effects(io_out, io_err) on_error<FileError, /log_file_error>]
main() {
  [auto] initial{lookup(40i32)}
  [auto] mapped{Result.map(initial, []([i32] value) { return(plus(value, 1i32)) })}
  [i32] selected{try(mapped)}
  return(selected)
}
)";
  }

  struct LocalAutoPublicationSnapshot {
    std::string bindingName;
    std::string bindingTypeText;
    std::string initializerResolvedPath;
    std::string initializerBindingTypeText;
    std::string initializerResultValueType;
    std::string initializerResultErrorType;
    bool initializerResultHasValue = false;
    bool initializerHasTry = false;
    uint64_t semanticNodeId = 0;

    bool operator==(const LocalAutoPublicationSnapshot &) const = default;
  };

  struct GraphLocalAutoShadowSnapshot {
    std::string formattedSemanticProduct;
    std::vector<LocalAutoPublicationSnapshot> localAutos;
  };

  const auto runWithShadowOptions = [&](bool legacyKeyShadow,
                                        bool legacySideChannelShadow,
                                        bool disableDependencyScratchPmr) {
    primec::Options options;
    options.inputPath = tempPath.string();
    options.entryPath = "/main";
    options.emitKind = "native";
    options.collectDiagnostics = true;
    options.benchmarkSemanticGraphLocalAutoLegacyKeyShadow = legacyKeyShadow;
    options.benchmarkSemanticGraphLocalAutoLegacySideChannelShadow =
        legacySideChannelShadow;
    options.benchmarkSemanticDisableGraphLocalAutoDependencyScratchPmr =
        disableDependencyScratchPmr;
    primec::addDefaultStdlibInclude(options.inputPath, options.importPaths);

    primec::CompilePipelineOutput output;
    primec::CompilePipelineDiagnosticInfo diagnosticInfo;
    primec::CompilePipelineErrorStage errorStage =
        primec::CompilePipelineErrorStage::None;
    std::string error;
    const bool ok =
        primec::runCompilePipeline(options, output, errorStage, error, &diagnosticInfo);

    REQUIRE(ok);
    CHECK(error.empty());
    CHECK(errorStage == primec::CompilePipelineErrorStage::None);
    CHECK_FALSE(output.hasFailure);
    REQUIRE(output.hasSemanticProgram);
    CHECK(diagnosticInfo.records.empty());

    GraphLocalAutoShadowSnapshot snapshot;
    snapshot.formattedSemanticProduct =
        primec::formatSemanticProgram(output.semanticProgram);

    for (const primec::SemanticProgramLocalAutoFact *entry :
         primec::semanticProgramLocalAutoFactView(output.semanticProgram)) {
      REQUIRE(entry != nullptr);
      if (entry->scopePath != "/main") {
        continue;
      }

      const std::string_view initializerResolvedPath =
          primec::semanticProgramLocalAutoFactInitializerResolvedPath(
              output.semanticProgram, *entry);
      const auto *bySemanticId =
          primec::semanticProgramLookupPublishedLocalAutoFactBySemanticId(
              output.semanticProgram, entry->semanticNodeId);
      REQUIRE(bySemanticId != nullptr);
      CHECK(bySemanticId->bindingName == entry->bindingName);
      CHECK(primec::semanticProgramLocalAutoFactInitializerResolvedPath(
                output.semanticProgram, *bySemanticId) == initializerResolvedPath);

      const auto initializerPathId =
          primec::semanticProgramLookupCallTargetStringId(output.semanticProgram,
                                                          initializerResolvedPath);
      const auto bindingNameId =
          primec::semanticProgramLookupCallTargetStringId(output.semanticProgram,
                                                          entry->bindingName);
      REQUIRE(initializerPathId.has_value());
      REQUIRE(bindingNameId.has_value());
      const auto *byInitPathAndName =
          primec::semanticProgramLookupPublishedLocalAutoFactByInitializerPathAndBindingNameId(
              output.semanticProgram, *initializerPathId, *bindingNameId);
      REQUIRE(byInitPathAndName != nullptr);
      CHECK(byInitPathAndName->semanticNodeId == entry->semanticNodeId);

      snapshot.localAutos.push_back(LocalAutoPublicationSnapshot{
          .bindingName = entry->bindingName,
          .bindingTypeText = entry->bindingTypeText,
          .initializerResolvedPath = std::string(initializerResolvedPath),
          .initializerBindingTypeText = entry->initializerBindingTypeText,
          .initializerResultValueType = entry->initializerResultValueType,
          .initializerResultErrorType = entry->initializerResultErrorType,
          .initializerResultHasValue = entry->initializerResultHasValue,
          .initializerHasTry = entry->initializerHasTry,
          .semanticNodeId = entry->semanticNodeId,
      });
    }

    std::sort(snapshot.localAutos.begin(),
              snapshot.localAutos.end(),
              [](const LocalAutoPublicationSnapshot &left,
                 const LocalAutoPublicationSnapshot &right) {
                return std::pair(left.bindingName, left.semanticNodeId) <
                       std::pair(right.bindingName, right.semanticNodeId);
              });
    return snapshot;
  };

  const GraphLocalAutoShadowSnapshot baseline =
      runWithShadowOptions(false, false, false);
  const GraphLocalAutoShadowSnapshot legacyKeyShadow =
      runWithShadowOptions(true, false, false);
  const GraphLocalAutoShadowSnapshot legacySideChannelShadow =
      runWithShadowOptions(false, true, false);
  const GraphLocalAutoShadowSnapshot dependencyScratchDisabled =
      runWithShadowOptions(false, false, true);
  const GraphLocalAutoShadowSnapshot combinedShadows =
      runWithShadowOptions(true, true, true);

  std::error_code ec;
  std::filesystem::remove(tempPath, ec);

  REQUIRE(baseline.localAutos.size() >= 2);
  CHECK(baseline.formattedSemanticProduct ==
        legacyKeyShadow.formattedSemanticProduct);
  CHECK(baseline.formattedSemanticProduct ==
        legacySideChannelShadow.formattedSemanticProduct);
  CHECK(baseline.formattedSemanticProduct ==
        dependencyScratchDisabled.formattedSemanticProduct);
  CHECK(baseline.formattedSemanticProduct ==
        combinedShadows.formattedSemanticProduct);
  CHECK(baseline.localAutos == legacyKeyShadow.localAutos);
  CHECK(baseline.localAutos == legacySideChannelShadow.localAutos);
  CHECK(baseline.localAutos == dependencyScratchDisabled.localAutos);
  CHECK(baseline.localAutos == combinedShadows.localAutos);

  const auto baselineInitial = std::find_if(
      baseline.localAutos.begin(),
      baseline.localAutos.end(),
      [](const LocalAutoPublicationSnapshot &entry) {
        return entry.bindingName == "initial";
      });
  const auto baselineMapped = std::find_if(
      baseline.localAutos.begin(),
      baseline.localAutos.end(),
      [](const LocalAutoPublicationSnapshot &entry) {
        return entry.bindingName == "mapped";
      });
  REQUIRE(baselineInitial != baseline.localAutos.end());
  REQUIRE(baselineMapped != baseline.localAutos.end());
  CHECK(baselineInitial->initializerResolvedPath == "/lookup");
  CHECK(baselineInitial->bindingTypeText == "Result<i32, FileError>");
  CHECK(baselineInitial->initializerResultHasValue);
  CHECK(baselineInitial->initializerResultValueType == "i32");
  CHECK(baselineInitial->initializerResultErrorType == "FileError");
  CHECK_FALSE(baselineInitial->initializerHasTry);
  CHECK(baselineMapped->bindingTypeText == "Result<i32, FileError>");
  CHECK_FALSE(baselineMapped->initializerResolvedPath.empty());
  CHECK_FALSE(baselineMapped->initializerHasTry);
}

TEST_CASE("lowering variadic reference-pack diagnostic exposes stable public payload") {
  const std::filesystem::path tempPath = makeTempIrPipelineSourcePath();
  {
    std::ofstream file(tempPath);
    REQUIRE(file.good());
    file << R"([return<i32>]
score([args<Reference<i32>>] values) {
  return(count(values))
}

[return<i32>]
main() {
  return(score(1i32, 2i32))
}
)";
  }

  primec::Options options;
  options.inputPath = tempPath.string();
  options.entryPath = "/main";
  options.emitKind = "vm";
  options.collectDiagnostics = true;
  primec::addDefaultStdlibInclude(options.inputPath, options.importPaths);

  primec::CompilePipelineOutput output;
  primec::CompilePipelineDiagnosticInfo diagnosticInfo;
  primec::CompilePipelineErrorStage errorStage =
      primec::CompilePipelineErrorStage::None;
  std::string error;
  REQUIRE(primec::runCompilePipeline(options, output, errorStage, error, &diagnosticInfo));
  CHECK(error.empty());
  REQUIRE(output.hasSemanticProgram);

  const primec::IrBackend *vmBackend = primec::findIrBackend("vm");
  REQUIRE(vmBackend != nullptr);

  primec::IrModule ir;
  primec::IrPreparationFailure failure;
  CHECK_FALSE(primec::prepareIrModule(output.program,
                                      &output.semanticProgram,
                                      options,
                                      vmBackend->validationTarget(options),
                                      ir,
                                      failure,
                                      &output.expandedSource));
  CHECK(failure.stage == primec::IrPreparationFailureStage::Lowering);
  CHECK(failure.message ==
        std::string(primec::VariadicArgsReferenceForwardingDiagnosticMessage));
  CHECK(failure.diagnosticInfo.message == failure.message);
  REQUIRE(failure.diagnosticInfo.hasPrimarySpan);
  CHECK(failure.diagnosticInfo.primarySpan.file ==
        std::filesystem::absolute(tempPath).string());
  CHECK(failure.diagnosticInfo.primarySpan.line == 8);
  CHECK(failure.diagnosticInfo.primarySpan.column == 16);
  CHECK(failure.diagnosticInfo.primarySpan.endLine == 8);
  CHECK(failure.diagnosticInfo.primarySpan.endColumn == 16);
  CHECK(failure.diagnosticInfo.relatedSpans.empty());

  const primec::CliFailure cliFailure =
      primec::describeIrPreparationFailure(failure, *vmBackend);
  CHECK(cliFailure.code == primec::DiagnosticCode::LoweringError);
  CHECK(cliFailure.notes == std::vector<std::string>{"backend: vm"});
  REQUIRE(cliFailure.diagnosticInfo.has_value());

  const primec::DiagnosticStabilityContract contract =
      primec::diagnosticStabilityContract(cliFailure.code, cliFailure.message);
  CHECK(contract.message == primec::DiagnosticStabilityTier::Stable);
  CHECK(contract.primarySpan == primec::DiagnosticStabilityTier::Stable);
  CHECK(contract.notes == primec::DiagnosticStabilityTier::Stable);

  std::ostringstream err;
  options.emitDiagnostics = true;
  CHECK(primec::emitCliFailure(err, options, cliFailure) == 2);
  const std::string publicJson = err.str();
  CHECK(publicJson.find("\"code\":\"PSC2001\"") != std::string::npos);
  CHECK(publicJson.find("\"message\":\"" +
                        std::string(primec::VariadicArgsReferenceForwardingDiagnosticMessage) +
                        "\"") != std::string::npos);
  CHECK(publicJson.find("\"line\":8") != std::string::npos);
  CHECK(publicJson.find("\"column\":16") != std::string::npos);
  CHECK(publicJson.find("\"notes\":[\"backend: vm\"]") != std::string::npos);

  std::error_code ec;
  std::filesystem::remove(tempPath, ec);
}

TEST_CASE("cli driver maps ir preparation failures through backend diagnostics") {
  const primec::IrBackend *vmBackend = primec::findIrBackend("vm");
  REQUIRE(vmBackend != nullptr);

  primec::IrPreparationFailure loweringFailure;
  loweringFailure.stage = primec::IrPreparationFailureStage::Lowering;
  loweringFailure.message = "native backend rejected entry";

  const primec::CliFailure loweringCliFailure = primec::describeIrPreparationFailure(loweringFailure, *vmBackend, 7);
  CHECK(loweringCliFailure.code == vmBackend->diagnostics().loweringDiagnosticCode);
  CHECK(loweringCliFailure.plainPrefix == vmBackend->diagnostics().loweringErrorPrefix);
  CHECK(loweringCliFailure.exitCode == 7);
  CHECK(loweringCliFailure.notes == std::vector<std::string>{"backend: vm"});
  CHECK(loweringCliFailure.message.find("vm backend") != std::string::npos);
  CHECK(loweringCliFailure.message.find("native backend") == std::string::npos);

  primec::IrPreparationFailure validationFailure;
  validationFailure.stage = primec::IrPreparationFailureStage::Validation;
  validationFailure.message = "bad ir";

  const primec::CliFailure validationCliFailure = primec::describeIrPreparationFailure(validationFailure, *vmBackend);
  CHECK(validationCliFailure.code == vmBackend->diagnostics().validationDiagnosticCode);
  CHECK(validationCliFailure.plainPrefix == vmBackend->diagnostics().validationErrorPrefix);
  CHECK(validationCliFailure.notes == std::vector<std::string>({"backend: vm", "stage: ir-validate"}));
  CHECK(validationCliFailure.message == "bad ir");
}

TEST_CASE("shared vm backend profile exposes canonical diagnostics") {
  const primec::IrBackendDiagnostics &diagnostics = primec::vmIrBackendDiagnostics();
  CHECK(diagnostics.loweringDiagnosticCode == primec::DiagnosticCode::LoweringError);
  CHECK(std::string_view(diagnostics.emitErrorPrefix) == "VM error: ");
  CHECK(std::string_view(diagnostics.backendTag) == "vm");

  primec::IrPreparationFailure loweringFailure;
  loweringFailure.stage = primec::IrPreparationFailureStage::Lowering;
  loweringFailure.message = "native backend rejected entry";
  const primec::CliFailure cliFailure =
      primec::describeIrPreparationFailure(loweringFailure, diagnostics, &primec::normalizeVmLoweringError, 5);
  CHECK(cliFailure.code == primec::DiagnosticCode::LoweringError);
  CHECK(cliFailure.exitCode == 5);
  CHECK(cliFailure.message.find("vm backend") != std::string::npos);

  std::string loweringError = "native backend rejected entry";
  primec::normalizeVmLoweringError(loweringError);
  CHECK(loweringError.find("vm backend") != std::string::npos);
  CHECK(loweringError.find("native backend") == std::string::npos);
}

TEST_SUITE_END();
