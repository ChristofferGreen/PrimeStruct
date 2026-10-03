#include "third_party/doctest.h"

#include "test_ir_pipeline_backends_registry_shared.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.backends.registry");

TEST_CASE("native pick call target sum resolution uses query facts") {
  const std::string source = R"(
[sum]
Choice {
  [i32] left
  [i32] right
}

[sum]
OtherChoice {
  [i32] left
  [i32] right
}

[return<Choice>]
makeChoice() {
  [Choice] choice{[right] 41i32}
  return(choice)
}

[return<i32>]
main() {
  return(pick(makeChoice()) {
    left(value) {
      plus(value, 1i32)
    }
    right(value) {
      plus(value, 2i32)
    }
  })
}
)";

  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidateThroughCompilePipeline(
      source, program, &semanticProgram, error, {}, {}));
  CHECK(error.empty());

  const auto semanticProductTargets =
      primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);
  const auto *makeChoiceQuery = findSemanticEntry(
      primec::semanticProgramQueryFactView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramQueryFact &entry) {
        return semanticTextOrFallback(semanticProgram, entry.scopePathId, entry.scopePath) == "/main" &&
               semanticTextOrFallback(semanticProgram, entry.callNameId, entry.callName) == "makeChoice";
      });
  REQUIRE(makeChoiceQuery != nullptr);
  CHECK(makeChoiceQuery->bindingTypeText == "/Choice");
  CHECK(primec::semanticProgramQueryFactResolvedPath(
            semanticProgram, *makeChoiceQuery) == "/makeChoice");
  const auto *makeChoiceReturn =
      primec::ir_lowerer::findSemanticProductReturnFactByPath(
          semanticProductTargets, "/makeChoice");
  REQUIRE(makeChoiceReturn != nullptr);
  CHECK(makeChoiceReturn->bindingTypeText == "Choice");

  primec::Options options;
  options.entryPath = "/main";
  primec::Program loweringProgram = program;
  primec::SemanticProgram loweringSemanticProgram = semanticProgram;
  primec::IrModule ir;
  primec::IrPreparationFailure failure;
  REQUIRE(primec::prepareIrModule(loweringProgram,
                                  &loweringSemanticProgram,
                                  options,
                                  primec::IrValidationTarget::Vm,
                                  ir,
                                  failure));
  CHECK(!ir.functions.empty());

  auto rewriteMakeChoiceQueryType =
      [](primec::SemanticProgram &semanticProduct, const std::string &typeText) {
        for (auto &fact : semanticProduct.queryFacts) {
          if (semanticTextOrFallback(semanticProduct, fact.scopePathId, fact.scopePath) == "/main" &&
              semanticTextOrFallback(semanticProduct, fact.callNameId, fact.callName) == "makeChoice") {
            fact.bindingTypeText = typeText;
            fact.queryTypeText = typeText;
            if (typeText.empty()) {
              fact.bindingTypeTextId = primec::InvalidSymbolId;
              fact.queryTypeTextId = primec::InvalidSymbolId;
            } else {
              fact.bindingTypeTextId =
                  primec::semanticProgramInternCallTargetString(semanticProduct, typeText);
              fact.queryTypeTextId =
                  primec::semanticProgramInternCallTargetString(semanticProduct, typeText);
            }
            return true;
          }
        }
        return false;
      };

  primec::Program staleProgram = program;
  primec::SemanticProgram staleSemanticProgram = semanticProgram;
  REQUIRE(rewriteMakeChoiceQueryType(staleSemanticProgram, "/OtherChoice"));

  primec::IrModule staleIr;
  primec::IrPreparationFailure staleFailure;
  CHECK_FALSE(primec::prepareIrModule(staleProgram,
                                      &staleSemanticProgram,
                                      options,
                                      primec::IrValidationTarget::Vm,
                                      staleIr,
                                      staleFailure));
  CHECK(staleFailure.stage == primec::IrPreparationFailureStage::Lowering);
  CHECK(staleFailure.message ==
        "stale semantic-product pick target query type: /main -> makeChoice");
  CHECK(staleFailure.diagnosticInfo.message == staleFailure.message);

  primec::Program incompleteProgram = program;
  primec::SemanticProgram incompleteSemanticProgram = semanticProgram;
  REQUIRE(rewriteMakeChoiceQueryType(incompleteSemanticProgram, ""));

  primec::IrModule incompleteIr;
  primec::IrPreparationFailure incompleteFailure;
  CHECK_FALSE(primec::prepareIrModule(incompleteProgram,
                                      &incompleteSemanticProgram,
                                      options,
                                      primec::IrValidationTarget::Vm,
                                      incompleteIr,
                                      incompleteFailure));
  CHECK(incompleteFailure.stage == primec::IrPreparationFailureStage::Lowering);
  CHECK(incompleteFailure.message ==
        "incomplete semantic-product pick target query fact: /main -> makeChoice");
  CHECK(incompleteFailure.diagnosticInfo.message == incompleteFailure.message);
}

TEST_CASE("native pick method target sum resolution uses query facts") {
  const std::string source = R"(
[sum]
Choice {
  [i32] left
  [i32] right
}

[sum]
OtherChoice {
  [i32] left
  [i32] right
}

[struct]
Picker {
  [i32] seed
}

[return<Choice>]
/Picker/makeChoice([Picker] self) {
  [Choice] choice{[right] self.seed}
  return(choice)
}

[return<i32>]
main() {
  [Picker] picker{Picker{41i32}}
  return(pick(picker.makeChoice()) {
    left(value) {
      plus(value, 1i32)
    }
    right(value) {
      plus(value, 2i32)
    }
  })
}
)";

  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidateThroughCompilePipeline(
      source, program, &semanticProgram, error, {}, {}));
  CHECK(error.empty());

  const auto semanticProductTargets =
      primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);
  const auto *makeChoiceQuery = findSemanticEntry(
      primec::semanticProgramQueryFactView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramQueryFact &entry) {
        return semanticTextOrFallback(semanticProgram, entry.scopePathId, entry.scopePath) == "/main" &&
               primec::semanticProgramQueryFactResolvedPath(
                   semanticProgram, entry) == "/Picker/makeChoice";
      });
  REQUIRE(makeChoiceQuery != nullptr);
  CHECK(makeChoiceQuery->bindingTypeText == "/Choice");
  const auto *makeChoiceReturn =
      primec::ir_lowerer::findSemanticProductReturnFactByPath(
          semanticProductTargets, "/Picker/makeChoice");
  REQUIRE(makeChoiceReturn != nullptr);
  CHECK(makeChoiceReturn->bindingTypeText == "Choice");

  primec::Options options;
  options.entryPath = "/main";
  primec::Program loweringProgram = program;
  primec::SemanticProgram loweringSemanticProgram = semanticProgram;
  primec::IrModule ir;
  primec::IrPreparationFailure failure;
  REQUIRE(primec::prepareIrModule(loweringProgram,
                                  &loweringSemanticProgram,
                                  options,
                                  primec::IrValidationTarget::Vm,
                                  ir,
                                  failure));
  CHECK(!ir.functions.empty());

  auto rewriteMakeChoiceQueryType =
      [](primec::SemanticProgram &semanticProduct, const std::string &typeText) {
        for (auto &fact : semanticProduct.queryFacts) {
          if (semanticTextOrFallback(semanticProduct, fact.scopePathId, fact.scopePath) == "/main" &&
              primec::semanticProgramQueryFactResolvedPath(
                  semanticProduct, fact) == "/Picker/makeChoice") {
            fact.bindingTypeText = typeText;
            fact.queryTypeText = typeText;
            if (typeText.empty()) {
              fact.bindingTypeTextId = primec::InvalidSymbolId;
              fact.queryTypeTextId = primec::InvalidSymbolId;
            } else {
              fact.bindingTypeTextId =
                  primec::semanticProgramInternCallTargetString(semanticProduct, typeText);
              fact.queryTypeTextId =
                  primec::semanticProgramInternCallTargetString(semanticProduct, typeText);
            }
            return true;
          }
        }
        return false;
      };

  primec::Program staleProgram = program;
  primec::SemanticProgram staleSemanticProgram = semanticProgram;
  REQUIRE(rewriteMakeChoiceQueryType(staleSemanticProgram, "/OtherChoice"));

  primec::IrModule staleIr;
  primec::IrPreparationFailure staleFailure;
  CHECK_FALSE(primec::prepareIrModule(staleProgram,
                                      &staleSemanticProgram,
                                      options,
                                      primec::IrValidationTarget::Vm,
                                      staleIr,
                                      staleFailure));
  CHECK(staleFailure.stage == primec::IrPreparationFailureStage::Lowering);
  CHECK(staleFailure.message ==
        "stale semantic-product pick target query type: /main -> makeChoice");
  CHECK(staleFailure.diagnosticInfo.message == staleFailure.message);

  primec::Program incompleteProgram = program;
  primec::SemanticProgram incompleteSemanticProgram = semanticProgram;
  REQUIRE(rewriteMakeChoiceQueryType(incompleteSemanticProgram, ""));

  primec::IrModule incompleteIr;
  primec::IrPreparationFailure incompleteFailure;
  CHECK_FALSE(primec::prepareIrModule(incompleteProgram,
                                      &incompleteSemanticProgram,
                                      options,
                                      primec::IrValidationTarget::Vm,
                                      incompleteIr,
                                      incompleteFailure));
  CHECK(incompleteFailure.stage == primec::IrPreparationFailureStage::Lowering);
  CHECK(incompleteFailure.message ==
        "incomplete semantic-product pick target query fact: /main -> makeChoice");
  CHECK(incompleteFailure.diagnosticInfo.message == incompleteFailure.message);
}

TEST_CASE("semantic product callable lookup prefers definition over same-path execution") {
  const std::string source =
      "[return<i32>]\n"
      "main() {\n"
      "  return(0i32)\n"
      "}\n"
      "\n"
      "[return<void>]\n"
      "log([i32] value) {\n"
      "  return()\n"
      "}\n"
      "\n"
      "log(1i32)\n";

  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidateThroughCompilePipeline(
      source, program, &semanticProgram, error, {}, {}));
  CHECK(error.empty());

  std::size_t logDefinitionSummaries = 0;
  std::size_t logExecutionSummaries = 0;
  for (const auto *summary :
       primec::semanticProgramCallableSummaryView(semanticProgram)) {
    REQUIRE(summary != nullptr);
    if (primec::semanticProgramCallableSummaryFullPath(
            semanticProgram, *summary) != "/log") {
      continue;
    }
    if (summary->isExecution) {
      ++logExecutionSummaries;
    } else {
      ++logDefinitionSummaries;
      CHECK(summary->returnKind == "void");
    }
  }
  CHECK(logDefinitionSummaries == 1);
  CHECK(logExecutionSummaries == 1);

  const auto *publishedLogSummary =
      primec::semanticProgramLookupPublishedCallableSummary(
          semanticProgram, "/log");
  REQUIRE(publishedLogSummary != nullptr);
  CHECK_FALSE(publishedLogSummary->isExecution);
  CHECK(publishedLogSummary->returnKind == "void");

  std::string resultMetadataError;
  CHECK(primec::ir_lowerer::validateSemanticProductResultMetadataCompleteness(
      &semanticProgram, resultMetadataError));
  CHECK(resultMetadataError.empty());

  primec::Options options;
  options.entryPath = "/main";
  primec::IrModule ir;
  primec::IrPreparationFailure failure;
  CHECK(primec::prepareIrModule(
      program, &semanticProgram, options, primec::IrValidationTarget::Vm, ir, failure));
}

TEST_CASE("compile pipeline freezes published semantic-product string scratch storage") {
  const std::filesystem::path tempPath = makeTempIrPipelineSourcePath();
  {
    std::ofstream file(tempPath);
    REQUIRE(file.good());
    file << R"(
[return<i32>]
helper([i32] input) {
  return(input)
}

[return<i32>]
main() {
  return(helper(21i32))
}
)";
  }

  primec::Options options;
  options.inputPath = tempPath.string();
  options.entryPath = "/main";
  options.emitKind = "vm";
  primec::addDefaultStdlibInclude(options.inputPath, options.importPaths);

  primec::CompilePipelineOutput output;
  primec::CompilePipelineErrorStage errorStage = primec::CompilePipelineErrorStage::None;
  std::string error;
  REQUIRE(primec::runCompilePipeline(options, output, errorStage, error));
  REQUIRE(output.hasSemanticProgram);

  std::error_code ec;
  std::filesystem::remove(tempPath, ec);

  CHECK(primec::semanticProgramPublishedStorageFrozen(output.semanticProgram));
  CHECK(output.semanticProgram.callTargetStringIdsByText.empty());

  const auto mainPathId =
      primec::semanticProgramLookupCallTargetStringId(output.semanticProgram, "/main");
  REQUIRE(mainPathId.has_value());
  CHECK(primec::semanticProgramResolveCallTargetString(output.semanticProgram, *mainPathId) ==
        "/main");

  const std::size_t stringCountBefore = output.semanticProgram.callTargetStringTable.size();
  CHECK(primec::semanticProgramInternCallTargetString(output.semanticProgram, "/late_mutation") ==
        primec::InvalidSymbolId);
  CHECK(output.semanticProgram.callTargetStringTable.size() == stringCountBefore);
}

TEST_CASE("ir lowerer requires semantic product before lowering") {
  primec::Program program;
  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, nullptr, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "semantic product is required for IR lowering");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("ir lowerer rejects semantic-product contract version mismatch") {
  primec::Program program;
  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  semanticProgram.contractVersion =
      primec::SemanticProductContractVersionCurrent + 1;

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "semantic-product contract version mismatch: expected 3, got 4");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("ir lowerer rejects semantic-product module artifact index overflow") {
  primec::Program program;
  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  primec::SemanticProgramModuleResolvedArtifacts moduleArtifacts;
  moduleArtifacts.identity.moduleKey = "/main";
  moduleArtifacts.directCallTargetIndices.push_back(0);
  semanticProgram.moduleResolvedArtifacts.push_back(std::move(moduleArtifacts));

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error ==
        "semantic-product contract module index out of range: family routing.direct-call, module /main, index 0");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("ir preparation helper reports lowering-stage failure for unresolved entry") {
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  primec::Options options;
  options.entryPath = "/missing";
  options.inlineIrCalls = true;

  primec::IrModule ir;
  primec::IrPreparationFailure failure;
  CHECK_FALSE(primec::prepareIrModule(program, &semanticProgram, options, primec::IrValidationTarget::Vm, ir, failure));
  CHECK(failure.stage == primec::IrPreparationFailureStage::Lowering);
  CHECK(failure.message == "native backend requires entry definition /missing");
  CHECK(failure.diagnosticInfo.message == failure.message);
}

TEST_CASE("semantic-product direct-call coverage conformance rejects missing targets for published definitions") {
  primec::Program program;

  primec::Definition callee;
  callee.fullPath = "/callee";
  callee.semanticNodeId = 82;
  program.definitions.push_back(callee);

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 81;
  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "callee";
  callExpr.semanticNodeId = 41;
  mainDef.statements.push_back(callExpr);
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  addVoidCallableSummary(semanticProgram, 81);
  addVoidCallableSummaryForPath(semanticProgram, "/callee", 82);
  const auto calleePathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/callee");
  semanticProgram.definitions.push_back(primec::SemanticProgramDefinition{
      .name = "callee",
      .fullPath = "/callee",
      .namespacePrefix = "",
      .sourceLine = 1,
      .sourceColumn = 1,
      .semanticNodeId = 82,
  });
  semanticProgram.publishedRoutingLookups.definitionIndicesByPathId.insert_or_assign(
      calleePathId, 0);

  std::string error;

  CHECK_FALSE(primec::ir_lowerer::validateSemanticProductDirectCallCoverage(
      program, &semanticProgram, error));
  CHECK(error == "missing semantic-product direct-call target: /main -> callee");
}

TEST_CASE("ir lowerer production entry rejects missing semantic-product direct-call targets") {
  primec::Program program;

  primec::Definition callee;
  callee.fullPath = "/callee";
  callee.semanticNodeId = 82;
  program.definitions.push_back(callee);

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 81;
  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "callee";
  callExpr.semanticNodeId = 41;
  mainDef.statements.push_back(callExpr);
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  addVoidCallableSummary(semanticProgram, 81);
  addVoidCallableSummaryForPath(semanticProgram, "/callee", 82);
  const auto calleePathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/callee");
  semanticProgram.definitions.push_back(primec::SemanticProgramDefinition{
      .name = "callee",
      .fullPath = "/callee",
      .namespacePrefix = "",
      .sourceLine = 1,
      .sourceColumn = 1,
      .semanticNodeId = 82,
  });
  semanticProgram.publishedRoutingLookups.definitionIndicesByPathId.insert_or_assign(
      calleePathId, 0);

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "missing semantic-product direct-call target: /main -> callee");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("semantic-product direct-call coverage conformance rejects missing semantic ids") {
  primec::Program program;

  primec::Definition callee;
  callee.fullPath = "/callee";
  callee.semanticNodeId = 82;
  program.definitions.push_back(callee);

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "callee";
  mainDef.statements.push_back(callExpr);
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";

  std::string error;

  CHECK_FALSE(primec::ir_lowerer::validateSemanticProductDirectCallCoverage(
      program, &semanticProgram, error));
  CHECK(error == "missing semantic-product direct-call semantic id: /main -> callee");
}

TEST_CASE("ir lowerer rejects stale semantic-product direct-call metadata") {
  primec::Program program;

  primec::Definition callee;
  callee.fullPath = "/callee";
  callee.semanticNodeId = 82;
  program.definitions.push_back(callee);

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 81;
  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "callee";
  callExpr.semanticNodeId = 41;
  mainDef.statements.push_back(callExpr);
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  addVoidCallableSummary(semanticProgram, 81);
  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      .scopePath = "/main",
      .callName = "callee",
      .sourceLine = 1,
      .sourceColumn = 1,
      .semanticNodeId = 41,
      .provenanceHandle = 0,
      .scopePathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
      .callNameId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "callee"),
      .resolvedPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/callee"),
      .stdlibSurfaceId = std::nullopt,
  });

  auto resetDirectCallMetadata = [&]() {
    auto &directCallTarget = semanticProgram.directCallTargets.back();
    directCallTarget.scopePathId =
        primec::semanticProgramInternCallTargetString(semanticProgram, "/main");
    directCallTarget.callNameId =
        primec::semanticProgramInternCallTargetString(semanticProgram, "callee");
    directCallTarget.resolvedPathId =
        primec::semanticProgramInternCallTargetString(semanticProgram, "/callee");
    semanticProgram.publishedRoutingLookups.directCallTargetIdsByExpr.insert_or_assign(
        41, directCallTarget.resolvedPathId);
  };

  std::string error;

  resetDirectCallMetadata();
  semanticProgram.directCallTargets.back().resolvedPathId = primec::InvalidSymbolId;
  CHECK_FALSE(primec::ir_lowerer::validateSemanticProductDirectCallCoverage(
      program, &semanticProgram, error));
  CHECK(error == "missing semantic-product direct-call resolved path id: /main -> callee");

  resetDirectCallMetadata();
  semanticProgram.directCallTargets.back().scopePathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/other");
  error.clear();
  CHECK_FALSE(primec::ir_lowerer::validateSemanticProductDirectCallCoverage(
      program, &semanticProgram, error));
  CHECK(error == "stale semantic-product direct-call scope metadata: /main -> callee");

  resetDirectCallMetadata();
  semanticProgram.directCallTargets.back().callNameId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "other");
  error.clear();
  CHECK_FALSE(primec::ir_lowerer::validateSemanticProductDirectCallCoverage(
      program, &semanticProgram, error));
  CHECK(error == "stale semantic-product direct-call name metadata: /main -> callee");

  resetDirectCallMetadata();
  semanticProgram.publishedRoutingLookups.directCallTargetIdsByExpr.insert_or_assign(
      41, primec::semanticProgramInternCallTargetString(semanticProgram, "/stale"));
  error.clear();
  CHECK_FALSE(primec::ir_lowerer::validateSemanticProductDirectCallCoverage(
      program, &semanticProgram, error));
  CHECK(error == "stale semantic-product direct-call target metadata: /main -> callee");
}

TEST_CASE("ir lowerer keeps semantic-product direct-call targets authoritative over rooted rewritten expr names") {
  primec::Program program;

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 81;
  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "/legacy";
  callExpr.semanticNodeId = 41;
  mainDef.statements.push_back(callExpr);
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  semanticProgram.callableSummaries.push_back(primec::SemanticProgramCallableSummary{
      .isExecution = false,
      .returnKind = "void",
      .isCompute = false,
      .isUnsafe = false,
      .activeEffects = {},
      .activeCapabilities = {},
      .hasResultType = false,
      .resultTypeHasValue = false,
      .resultValueType = "",
      .resultErrorType = "",
      .hasOnError = false,
      .onErrorHandlerPath = "",
      .onErrorErrorType = "",
      .onErrorBoundArgCount = 0,
      .semanticNodeId = 81,
      .provenanceHandle = 0,
      .fullPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
  });
  semanticProgram.returnFacts.push_back(primec::SemanticProgramReturnFact{
      .returnKind = "void",
      .structPath = "",
      .bindingTypeText = "void",
      .isMutable = false,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 81,
      .definitionPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
  });
  semanticProgram.publishedRoutingLookups.returnFactIndicesByDefinitionId
      .insert_or_assign(81, semanticProgram.returnFacts.size() - 1);
  publishFixtureCallableAndReturnRouting(semanticProgram);
  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      .scopePath = "/main",
      .callName = "/legacy",
      .sourceLine = 1,
      .sourceColumn = 1,
      .semanticNodeId = 41,
      .scopePathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
      .callNameId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/legacy"),
      .resolvedPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/semantic/target"),
      .stdlibSurfaceId = std::nullopt,
  });
  semanticProgram.publishedRoutingLookups.directCallTargetIdsByExpr.insert_or_assign(
      41, semanticProgram.directCallTargets.back().resolvedPathId);

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error.find("native backend only supports arithmetic/comparison/clamp/min/max/abs/sign/"
                   "saturate/convert/pointer/assign/increment/decrement calls in expressions") !=
        std::string::npos);
  CHECK(error.find("call=/semantic/target") != std::string::npos);
  CHECK(error.find("name=/legacy") != std::string::npos);
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("semantic-product method-call coverage conformance rejects missing targets") {
  primec::Program program;

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 81;
  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "values";
  primec::Expr methodCallExpr;
  methodCallExpr.kind = primec::Expr::Kind::Call;
  methodCallExpr.name = "count";
  methodCallExpr.isMethodCall = true;
  methodCallExpr.semanticNodeId = 42;
  methodCallExpr.args.push_back(receiverExpr);
  mainDef.statements.push_back(methodCallExpr);
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";

  std::string error;

  CHECK_FALSE(primec::ir_lowerer::validateSemanticProductMethodCallCoverage(
      program, &semanticProgram, error));
  CHECK(error == "missing semantic-product method-call target: /main -> count");
}

TEST_CASE("semantic-product method-call coverage conformance rejects missing semantic ids") {
  primec::Program program;

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "values";
  primec::Expr methodCallExpr;
  methodCallExpr.kind = primec::Expr::Kind::Call;
  methodCallExpr.name = "count";
  methodCallExpr.isMethodCall = true;
  methodCallExpr.args.push_back(receiverExpr);
  mainDef.statements.push_back(methodCallExpr);
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";

  std::string error;

  CHECK_FALSE(primec::ir_lowerer::validateSemanticProductMethodCallCoverage(
      program, &semanticProgram, error));
  CHECK(error == "missing semantic-product method-call semantic id: /main -> count");
}

TEST_CASE("method-call coverage rejects missing resolved path ids before lookup gaps") {
  primec::Program program;

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 4200;
  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "values";
  primec::Expr methodCallExpr;
  methodCallExpr.kind = primec::Expr::Kind::Call;
  methodCallExpr.name = "count";
  methodCallExpr.isMethodCall = true;
  methodCallExpr.semanticNodeId = 4201;
  methodCallExpr.args.push_back(receiverExpr);
  mainDef.statements.push_back(methodCallExpr);
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  addVoidCallableSummary(semanticProgram, 4200);
  semanticProgram.methodCallTargets.push_back(primec::SemanticProgramMethodCallTarget{
      .scopePath = "/main",
      .methodName = "count",
      .receiverTypeText = "vector<i32>",
      .sourceLine = 1,
      .sourceColumn = 1,
      .semanticNodeId = 4201,
      .resolvedPathId = primec::InvalidSymbolId,
      .stdlibSurfaceId = std::nullopt,
  });
  semanticProgram.publishedRoutingLookups.methodCallTargetIdsByExpr.insert_or_assign(
      4201, primec::semanticProgramInternCallTargetString(semanticProgram,
                                                          "/std/collections/vector/count"));

  std::string error;

  CHECK_FALSE(primec::ir_lowerer::validateSemanticProductMethodCallCoverage(
      program, &semanticProgram, error));
  CHECK(error == "missing semantic-product method-call resolved path id: /main -> count");
}

TEST_CASE("ir lowerer rejects stale semantic-product method-call metadata") {
  primec::Program program;

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 4200;
  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "values";
  primec::Expr methodCallExpr;
  methodCallExpr.kind = primec::Expr::Kind::Call;
  methodCallExpr.name = "count";
  methodCallExpr.isMethodCall = true;
  methodCallExpr.semanticNodeId = 4201;
  methodCallExpr.args.push_back(receiverExpr);
  mainDef.statements.push_back(methodCallExpr);
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  addVoidCallableSummary(semanticProgram, 4200);
  semanticProgram.methodCallTargets.push_back(primec::SemanticProgramMethodCallTarget{
      .scopePath = "/main",
      .methodName = "count",
      .receiverTypeText = "vector<i32>",
      .sourceLine = 1,
      .sourceColumn = 1,
      .semanticNodeId = 4201,
      .provenanceHandle = 0,
      .scopePathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
      .methodNameId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "count"),
      .receiverTypeTextId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "vector<i32>"),
      .resolvedPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram,
                                                        "/std/collections/vector/count"),
      .stdlibSurfaceId = std::nullopt,
  });

  auto resetMethodCallMetadata = [&]() {
    auto &methodCallTarget = semanticProgram.methodCallTargets.back();
    methodCallTarget.scopePathId =
        primec::semanticProgramInternCallTargetString(semanticProgram, "/main");
    methodCallTarget.methodNameId =
        primec::semanticProgramInternCallTargetString(semanticProgram, "count");
    methodCallTarget.receiverTypeTextId =
        primec::semanticProgramInternCallTargetString(semanticProgram, "vector<i32>");
    methodCallTarget.resolvedPathId =
        primec::semanticProgramInternCallTargetString(semanticProgram,
                                                      "/std/collections/vector/count");
    semanticProgram.publishedRoutingLookups.methodCallTargetIdsByExpr.insert_or_assign(
        4201, methodCallTarget.resolvedPathId);
  };

  std::string error;

  resetMethodCallMetadata();
  semanticProgram.methodCallTargets.back().scopePathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/other");
  CHECK_FALSE(primec::ir_lowerer::validateSemanticProductMethodCallCoverage(
      program, &semanticProgram, error));
  CHECK(error == "stale semantic-product method-call scope metadata: /main -> count");

  resetMethodCallMetadata();
  semanticProgram.methodCallTargets.back().methodNameId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "other");
  error.clear();
  CHECK_FALSE(primec::ir_lowerer::validateSemanticProductMethodCallCoverage(
      program, &semanticProgram, error));
  CHECK(error == "stale semantic-product method-call name metadata: /main -> count");

  resetMethodCallMetadata();
  semanticProgram.methodCallTargets.back().receiverTypeTextId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "vector<i64>");
  error.clear();
  CHECK_FALSE(primec::ir_lowerer::validateSemanticProductMethodCallCoverage(
      program, &semanticProgram, error));
  CHECK(error == "stale semantic-product method-call receiver metadata: /main -> count");

  resetMethodCallMetadata();
  semanticProgram.publishedRoutingLookups.methodCallTargetIdsByExpr.insert_or_assign(
      4201, primec::semanticProgramInternCallTargetString(semanticProgram, "/stale"));
  error.clear();
  CHECK_FALSE(primec::ir_lowerer::validateSemanticProductMethodCallCoverage(
      program, &semanticProgram, error));
  CHECK(error == "stale semantic-product method-call target metadata: /main -> count");
}

TEST_CASE("ir lowerer production entry rejects missing semantic-product method-call targets") {
  primec::Program program;

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 4200;
  primec::Expr valuesExpr;
  valuesExpr.kind = primec::Expr::Kind::Name;
  valuesExpr.name = "values";
  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.isMethodCall = true;
  callExpr.name = "count";
  callExpr.semanticNodeId = 4201;
  callExpr.args.push_back(valuesExpr);
  mainDef.statements.push_back(callExpr);
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  addVoidCallableSummary(semanticProgram, 4200);

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "missing semantic-product method-call target: /main -> count");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("ir lowerer rejects missing semantic-product bridge-path choices") {
  primec::Program program;

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 5203;
  primec::Expr valuesExpr;
  valuesExpr.kind = primec::Expr::Kind::Name;
  valuesExpr.name = "values";
  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "count";
  callExpr.semanticNodeId = 52;
  callExpr.args.push_back(valuesExpr);
  mainDef.statements.push_back(callExpr);
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  addVoidCallableSummary(semanticProgram, 5203);
  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      .scopePath = "/main",
      .callName = "count",
      .sourceLine = 1,
      .sourceColumn = 1,
      .semanticNodeId = 52,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/vector/count"),
      .stdlibSurfaceId = primec::StdlibSurfaceId::CollectionsManifestSurface0,
  });
  semanticProgram.publishedRoutingLookups.directCallTargetIdsByExpr.insert_or_assign(
      52, semanticProgram.directCallTargets.back().resolvedPathId);
  semanticProgram.publishedRoutingLookups.directCallStdlibSurfaceIdsByExpr.insert_or_assign(
      52, primec::StdlibSurfaceId::CollectionsManifestSurface0);

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "missing semantic-product bridge-path choice: /main -> count");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("bridge-path coverage rejects missing helper name ids before lookup gaps") {
  primec::Program program;

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 5200;
  primec::Expr valuesExpr;
  valuesExpr.kind = primec::Expr::Kind::Name;
  valuesExpr.name = "values";
  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "count";
  callExpr.semanticNodeId = 5201;
  callExpr.args.push_back(valuesExpr);
  mainDef.statements.push_back(callExpr);
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  addVoidCallableSummary(semanticProgram, 5200);
  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      .scopePath = "/main",
      .callName = "count",
      .sourceLine = 1,
      .sourceColumn = 1,
      .semanticNodeId = 5201,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/vector/count"),
      .stdlibSurfaceId = std::nullopt,
  });
  semanticProgram.bridgePathChoices.push_back(primec::SemanticProgramBridgePathChoice{
      .scopePath = "/main",
      .collectionFamily = "vector",
      .sourceLine = 1,
      .sourceColumn = 1,
      .semanticNodeId = 5201,
      .helperNameId = primec::InvalidSymbolId,
      .chosenPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/vector/count"),
      .stdlibSurfaceId = primec::StdlibSurfaceId::CollectionsManifestSurface0,
  });
  semanticProgram.publishedRoutingLookups.bridgePathChoiceIdsByExpr.insert_or_assign(
      5201, semanticProgram.bridgePathChoices.back().chosenPathId);

  std::string error;

  CHECK_FALSE(primec::ir_lowerer::validateSemanticProductBridgePathCoverage(
      program, &semanticProgram, error));
  CHECK(error == "missing semantic-product bridge helper name id: /main -> count");
}

TEST_CASE("bridge-path coverage rejects helper name ids before direct-call target gaps") {
  primec::Program program;

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 5204;
  primec::Expr valuesExpr;
  valuesExpr.kind = primec::Expr::Kind::Name;
  valuesExpr.name = "values";
  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "count";
  callExpr.semanticNodeId = 5202;
  callExpr.args.push_back(valuesExpr);
  mainDef.statements.push_back(callExpr);
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  addVoidCallableSummary(semanticProgram, 5204);
  semanticProgram.bridgePathChoices.push_back(primec::SemanticProgramBridgePathChoice{
      .scopePath = "/main",
      .collectionFamily = "vector",
      .sourceLine = 1,
      .sourceColumn = 1,
      .semanticNodeId = 5202,
      .helperNameId = primec::InvalidSymbolId,
      .chosenPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/vector/count"),
      .stdlibSurfaceId = primec::StdlibSurfaceId::CollectionsManifestSurface0,
  });
  semanticProgram.publishedRoutingLookups.bridgePathChoiceIdsByExpr.insert_or_assign(
      5202, semanticProgram.bridgePathChoices.back().chosenPathId);

  std::string error;

  CHECK_FALSE(primec::ir_lowerer::validateSemanticProductBridgePathCoverage(
      program, &semanticProgram, error));
  CHECK(error == "missing semantic-product bridge helper name id: /main -> count");
}

TEST_CASE("ir lowerer production entry reports native diagnostic without bridge-path choice") {
  primec::Program program;

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 5206;
  primec::Expr valuesExpr;
  valuesExpr.kind = primec::Expr::Kind::Name;
  valuesExpr.name = "values";
  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "contains";
  callExpr.semanticNodeId = 5207;
  callExpr.args.push_back(valuesExpr);
  mainDef.statements.push_back(callExpr);
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  addVoidCallableSummary(semanticProgram, 5206);
  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      .scopePath = "/main",
      .callName = "contains",
      .sourceLine = 1,
      .sourceColumn = 1,
      .semanticNodeId = 5207,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(
          semanticProgram, "/std/collections/map/contains"),
      .stdlibSurfaceId = primec::findStdlibSurfaceMetadataByBridgeKey("collections.map_helpers")->id,
  });
  semanticProgram.publishedRoutingLookups.directCallTargetIdsByExpr.insert_or_assign(
      5207, semanticProgram.directCallTargets.front().resolvedPathId);

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error.find("native backend only supports arithmetic/comparison/clamp/min/max/abs/sign/"
                   "saturate/convert/pointer/assign/increment/decrement calls in expressions") !=
        std::string::npos);
  CHECK(error.find("call=/std/collections/map/contains") != std::string::npos);
  CHECK(error.find("name=contains") != std::string::npos);
  CHECK(diagnosticInfo.message == error);
}

TEST_SUITE_END();
