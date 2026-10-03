#include "test_ir_pipeline_validation_callback_factories.h"
#include "primec/testing/IrLowererCollectionSurfaceContracts.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("ir lowerer semantic-product index does not expose on_error path fallback") {
  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 0;

  primec::SemanticProgram semanticProgram;
  semanticProgram.onErrorFacts.push_back(primec::SemanticProgramOnErrorFact{
      .definitionPath = "/main",
      .returnKind = "return",
      .errorType = "FileError",
      .boundArgCount = 1,
      .boundArgTexts = {"value"},
      .returnResultHasValue = false,
      .returnResultValueType = "",
      .returnResultErrorType = "",
      .semanticNodeId = 0,
      .definitionPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
      .returnKindId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "return"),
      .handlerPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/handler"),
      .errorTypeId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "FileError"),
      .boundArgTextIds = {
          primec::semanticProgramInternCallTargetString(semanticProgram, "value"),
      },
  });

  const auto semanticIndex =
      primec::ir_lowerer::buildSemanticProductIndex(&semanticProgram);
  const auto mainPathId =
      primec::semanticProgramLookupCallTargetStringId(semanticProgram, "/main");
  REQUIRE(mainPathId.has_value());
  CHECK(semanticIndex.onErrorFactsByDefinitionId.empty());
  const auto *onErrorFact =
      primec::ir_lowerer::findSemanticProductOnErrorFact(
          &semanticProgram, semanticIndex, mainDef);
  CHECK(onErrorFact == nullptr);
}

TEST_CASE("ir lowerer semantic-product adapter uses on_error semantic-id matches without path fallback") {
  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 5201;

  primec::SemanticProgram semanticProgram;
  const primec::SymbolId mainPathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/main");
  semanticProgram.onErrorFacts.push_back(primec::SemanticProgramOnErrorFact{
      .definitionPath = "/main",
      .returnKind = "return",
      .errorType = "FileError",
      .boundArgCount = 1,
      .boundArgTexts = {"value"},
      .returnResultHasValue = false,
      .returnResultValueType = "",
      .returnResultErrorType = "",
      .semanticNodeId = 5201,
      .definitionPathId = mainPathId,
      .returnKindId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "return"),
      .handlerPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/handler"),
      .errorTypeId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "FileError"),
      .boundArgTextIds = {
          primec::semanticProgramInternCallTargetString(semanticProgram, "value"),
      },
  });
  semanticProgram.onErrorFacts.push_back(primec::SemanticProgramOnErrorFact{
      .definitionPath = "/main",
      .returnKind = "return",
      .errorType = "OtherError",
      .boundArgCount = 2,
      .boundArgTexts = {"stale", "value"},
      .returnResultHasValue = false,
      .returnResultValueType = "",
      .returnResultErrorType = "",
      .semanticNodeId = 0,
      .definitionPathId = mainPathId,
      .returnKindId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "return"),
      .handlerPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/stale_handler"),
      .errorTypeId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "OtherError"),
      .boundArgTextIds = {
          primec::semanticProgramInternCallTargetString(semanticProgram, "stale"),
          primec::semanticProgramInternCallTargetString(semanticProgram, "value"),
      },
  });
  semanticProgram.publishedRoutingLookups.onErrorFactIndicesByDefinitionId
      .insert_or_assign(5201, 0);
  semanticProgram.publishedRoutingLookups.onErrorFactIndicesByDefinitionPathId
      .insert_or_assign(mainPathId, 1);

  const auto semanticTargets =
      primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);
  const auto *onErrorFact = primec::ir_lowerer::findSemanticProductOnErrorFact(semanticTargets, mainDef);
  REQUIRE(onErrorFact != nullptr);
  CHECK(onErrorFact->semanticNodeId == 5201);
  CHECK(onErrorFact->errorType == "FileError");
  CHECK(onErrorFact->boundArgCount == 1);
}

TEST_CASE("ir lowerer semantic-product adapter ignores local-auto initializer-path fallback") {
  primec::Expr initCall;
  initCall.kind = primec::Expr::Kind::Call;
  initCall.name = "id";
  initCall.semanticNodeId = 7101;

  primec::Expr localBinding = primec::validation_test_support::makeBindingNameExpr("value");
  localBinding.semanticNodeId = 0;
  localBinding.args = {initCall};

  primec::SemanticProgram semanticProgram;
  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      .scopePath = "/main",
      .callName = "id",
      .sourceLine = 12,
      .sourceColumn = 5,
      .semanticNodeId = 7101,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/id"),
      .stdlibSurfaceId = std::nullopt,
  });
  const primec::SymbolId bindingNameId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "value");
  const primec::SymbolId initializerPathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/id");
  semanticProgram.localAutoFacts.push_back(primec::SemanticProgramLocalAutoFact{
      .scopePath = "/main",
      .bindingName = "value",
      .bindingTypeText = "i32",
      .initializerBindingTypeText = "i32",
      .initializerReceiverBindingTypeText = "",
      .initializerQueryTypeText = "",
      .initializerResultHasValue = false,
      .initializerResultValueType = "",
      .initializerResultErrorType = "",
      .initializerHasTry = false,
      .initializerTryOperandResolvedPath = "",
      .initializerTryOperandBindingTypeText = "",
      .initializerTryOperandReceiverBindingTypeText = "",
      .initializerTryOperandQueryTypeText = "",
      .initializerTryValueType = "",
      .initializerTryErrorType = "",
      .initializerTryContextReturnKind = "",
      .initializerTryOnErrorHandlerPath = "",
      .initializerTryOnErrorErrorType = "",
      .initializerTryOnErrorBoundArgCount = 0,
      .sourceLine = 12,
      .sourceColumn = 3,
      .semanticNodeId = 0,
      .provenanceHandle = 0,
      .initializerDirectCallResolvedPath = "",
      .initializerDirectCallReturnKind = "",
      .initializerMethodCallResolvedPath = "",
      .initializerMethodCallReturnKind = "",
      .initializerStdlibSurfaceId = std::nullopt,
      .initializerDirectCallStdlibSurfaceId = std::nullopt,
      .initializerMethodCallStdlibSurfaceId = std::nullopt,
      .bindingNameId = bindingNameId,
      .initializerResolvedPathId = initializerPathId,
  });
  semanticProgram.publishedRoutingLookups.localAutoFactIndicesByInitPathAndBindingNameId
      .insert_or_assign((static_cast<uint64_t>(initializerPathId) << 32) |
                            static_cast<uint64_t>(bindingNameId),
                        0);

  const auto adapter = primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);
  CHECK(adapter.semanticIndex.localAutoFactsByExpr.empty());
  const auto *localAutoFact = primec::ir_lowerer::findSemanticProductLocalAutoFact(adapter, localBinding);
  CHECK(localAutoFact == nullptr);
}

TEST_CASE("ir lowerer semantic-product index does not expose local-auto path fallback") {
  primec::Expr initCall;
  initCall.kind = primec::Expr::Kind::Call;
  initCall.name = "id";
  initCall.semanticNodeId = 7301;

  primec::Expr localBinding = primec::validation_test_support::makeBindingNameExpr("value");
  localBinding.semanticNodeId = 0;
  localBinding.args = {initCall};

  primec::SemanticProgram semanticProgram;
  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      .scopePath = "/main",
      .callName = "id",
      .sourceLine = 12,
      .sourceColumn = 5,
      .semanticNodeId = 7301,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/id"),
      .stdlibSurfaceId = std::nullopt,
  });
  const primec::SymbolId bindingNameId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "value");
  const primec::SymbolId initializerPathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/id");
  semanticProgram.localAutoFacts.push_back(primec::SemanticProgramLocalAutoFact{
      .scopePath = "/main",
      .bindingName = "value",
      .bindingTypeText = "i32",
      .initializerBindingTypeText = "i32",
      .initializerReceiverBindingTypeText = "",
      .initializerQueryTypeText = "",
      .initializerResultHasValue = false,
      .initializerResultValueType = "",
      .initializerResultErrorType = "",
      .initializerHasTry = false,
      .initializerTryOperandResolvedPath = "",
      .initializerTryOperandBindingTypeText = "",
      .initializerTryOperandReceiverBindingTypeText = "",
      .initializerTryOperandQueryTypeText = "",
      .initializerTryValueType = "",
      .initializerTryErrorType = "",
      .initializerTryContextReturnKind = "",
      .initializerTryOnErrorHandlerPath = "",
      .initializerTryOnErrorErrorType = "",
      .initializerTryOnErrorBoundArgCount = 0,
      .sourceLine = 12,
      .sourceColumn = 3,
      .semanticNodeId = 0,
      .provenanceHandle = 0,
      .initializerDirectCallResolvedPath = "",
      .initializerDirectCallReturnKind = "",
      .initializerMethodCallResolvedPath = "",
      .initializerMethodCallReturnKind = "",
      .initializerStdlibSurfaceId = std::nullopt,
      .initializerDirectCallStdlibSurfaceId = std::nullopt,
      .initializerMethodCallStdlibSurfaceId = std::nullopt,
      .bindingNameId = bindingNameId,
      .initializerResolvedPathId = initializerPathId,
  });

  const auto semanticIndex =
      primec::ir_lowerer::buildSemanticProductIndex(&semanticProgram);
  CHECK(semanticIndex.localAutoFactsByExpr.empty());
  const auto *localAutoFact =
      primec::ir_lowerer::findSemanticProductLocalAutoFact(
          &semanticProgram, semanticIndex, localBinding);
  CHECK(localAutoFact == nullptr);
}

TEST_CASE("ir lowerer semantic-product adapter uses local-auto semantic-id matches without path fallback") {
  primec::Expr initCall;
  initCall.kind = primec::Expr::Kind::Call;
  initCall.name = "id";
  initCall.semanticNodeId = 7201;

  primec::Expr localBinding = primec::validation_test_support::makeBindingNameExpr("value");
  localBinding.semanticNodeId = 7202;
  localBinding.args = {initCall};

  primec::SemanticProgram semanticProgram;
  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      .scopePath = "/main",
      .callName = "id",
      .sourceLine = 16,
      .sourceColumn = 5,
      .semanticNodeId = 7201,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/id"),
      .stdlibSurfaceId = std::nullopt,
  });

  const primec::SymbolId bindingNameId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "value");
  const primec::SymbolId initializerPathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/id");

  primec::SemanticProgramLocalAutoFact semanticIdFact{
      .scopePath = "/main",
      .bindingName = "value",
      .bindingTypeText = "i32",
      .initializerBindingTypeText = "i32",
      .initializerReceiverBindingTypeText = "",
      .initializerQueryTypeText = "",
      .initializerResultHasValue = false,
      .initializerResultValueType = "",
      .initializerResultErrorType = "",
      .initializerHasTry = false,
      .initializerTryOperandResolvedPath = "",
      .initializerTryOperandBindingTypeText = "",
      .initializerTryOperandReceiverBindingTypeText = "",
      .initializerTryOperandQueryTypeText = "",
      .initializerTryValueType = "",
      .initializerTryErrorType = "",
      .initializerTryContextReturnKind = "",
      .initializerTryOnErrorHandlerPath = "",
      .initializerTryOnErrorErrorType = "",
      .initializerTryOnErrorBoundArgCount = 0,
      .sourceLine = 16,
      .sourceColumn = 3,
      .semanticNodeId = 7202,
      .provenanceHandle = 0,
      .initializerDirectCallResolvedPath = "",
      .initializerDirectCallReturnKind = "",
      .initializerMethodCallResolvedPath = "",
      .initializerMethodCallReturnKind = "",
      .initializerStdlibSurfaceId = std::nullopt,
      .initializerDirectCallStdlibSurfaceId = std::nullopt,
      .initializerMethodCallStdlibSurfaceId = std::nullopt,
      .bindingNameId = bindingNameId,
      .initializerResolvedPathId = initializerPathId,
  };
  semanticProgram.localAutoFacts.push_back(std::move(semanticIdFact));

  primec::SemanticProgramLocalAutoFact fallbackFact{
      .scopePath = "/main",
      .bindingName = "value",
      .bindingTypeText = "f64",
      .initializerBindingTypeText = "f64",
      .initializerReceiverBindingTypeText = "",
      .initializerQueryTypeText = "",
      .initializerResultHasValue = false,
      .initializerResultValueType = "",
      .initializerResultErrorType = "",
      .initializerHasTry = false,
      .initializerTryOperandResolvedPath = "",
      .initializerTryOperandBindingTypeText = "",
      .initializerTryOperandReceiverBindingTypeText = "",
      .initializerTryOperandQueryTypeText = "",
      .initializerTryValueType = "",
      .initializerTryErrorType = "",
      .initializerTryContextReturnKind = "",
      .initializerTryOnErrorHandlerPath = "",
      .initializerTryOnErrorErrorType = "",
      .initializerTryOnErrorBoundArgCount = 0,
      .sourceLine = 16,
      .sourceColumn = 3,
      .semanticNodeId = 0,
      .provenanceHandle = 0,
      .initializerDirectCallResolvedPath = "",
      .initializerDirectCallReturnKind = "",
      .initializerMethodCallResolvedPath = "",
      .initializerMethodCallReturnKind = "",
      .initializerStdlibSurfaceId = std::nullopt,
      .initializerDirectCallStdlibSurfaceId = std::nullopt,
      .initializerMethodCallStdlibSurfaceId = std::nullopt,
      .bindingNameId = bindingNameId,
      .initializerResolvedPathId = initializerPathId,
  };
  semanticProgram.localAutoFacts.push_back(std::move(fallbackFact));
  semanticProgram.publishedRoutingLookups.localAutoFactIndicesByExpr.insert_or_assign(7202, 0);
  semanticProgram.publishedRoutingLookups.localAutoFactIndicesByInitPathAndBindingNameId
      .insert_or_assign((static_cast<uint64_t>(initializerPathId) << 32) |
                            static_cast<uint64_t>(bindingNameId),
                        1);

  const auto adapter = primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);
  const auto *localAutoFact = primec::ir_lowerer::findSemanticProductLocalAutoFact(adapter, localBinding);
  REQUIRE(localAutoFact != nullptr);
  CHECK(localAutoFact->semanticNodeId == 7202);
  CHECK(localAutoFact->bindingTypeText == "i32");
}

TEST_CASE("ir lowerer semantic-product index requires published binding semantic-id maps") {
  primec::SemanticProgram semanticProgram;
  semanticProgram.bindingFacts.push_back(primec::SemanticProgramBindingFact{
      .scopePath = "/main",
      .siteKind = "local",
      .name = "selected",
      .bindingTypeText = "i32",
      .isMutable = false,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .sourceLine = 14,
      .sourceColumn = 7,
      .semanticNodeId = 7401,
      .resolvedPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/selected"),
  });

  primec::Expr bindingExpr = primec::validation_test_support::makeBindingNameExpr("selected");
  bindingExpr.semanticNodeId = 7401;

  const auto semanticIndex =
      primec::ir_lowerer::buildSemanticProductIndex(&semanticProgram);
  CHECK(semanticIndex.bindingFactsByExpr.empty());
  const auto *rawOnlyBindingFact =
      primec::ir_lowerer::findSemanticProductBindingFact(semanticIndex, bindingExpr);
  CHECK(rawOnlyBindingFact == nullptr);

  semanticProgram.publishedRoutingLookups.bindingFactIndicesByExpr.insert_or_assign(7401, 0);
  const auto mappedSemanticIndex =
      primec::ir_lowerer::buildSemanticProductIndex(&semanticProgram);
  CHECK(mappedSemanticIndex.bindingFactsByExpr.count(7401) == 1);
  const auto *bindingFact =
      primec::ir_lowerer::findSemanticProductBindingFact(mappedSemanticIndex, bindingExpr);
  REQUIRE(bindingFact != nullptr);
  CHECK(bindingFact->bindingTypeText == "i32");
}

TEST_CASE("ir lowerer semantic-product adapter ignores binding scope-name fallback") {
  primec::SemanticProgram semanticProgram;
  semanticProgram.bindingFacts.push_back(primec::SemanticProgramBindingFact{
      .scopePath = "/main",
      .siteKind = "local",
      .name = "selected",
      .bindingTypeText = "Choice",
      .isMutable = false,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .sourceLine = 14,
      .sourceColumn = 7,
      .semanticNodeId = 7401,
      .resolvedPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/selected"),
  });
  semanticProgram.publishedRoutingLookups.bindingFactIndicesByExpr.insert_or_assign(7401, 0);

  primec::Expr staleBindingExpr;
  staleBindingExpr.kind = primec::Expr::Kind::Name;
  staleBindingExpr.name = "selected";
  staleBindingExpr.semanticNodeId = 7402;

  const auto adapter = primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);
  CHECK(adapter.semanticIndex.bindingFactsByExpr.count(7401) == 1);
  const auto *bindingFact =
      primec::ir_lowerer::findSemanticProductBindingFact(adapter, staleBindingExpr);
  CHECK(bindingFact == nullptr);
}

TEST_CASE("ir lowerer statement binding helper consumes semantic-product index directly") {
  primec::SemanticProgram semanticProgram;
  semanticProgram.bindingFacts.push_back(primec::SemanticProgramBindingFact{
      .scopePath = "/main",
      .siteKind = "local",
      .name = "selected",
      .bindingTypeText = "",
      .isMutable = false,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .sourceLine = 21,
      .sourceColumn = 5,
      .semanticNodeId = 7501,
      .resolvedPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/selected"),
      .bindingTypeTextId =
          primec::semanticProgramInternCallTargetString(
              semanticProgram, "map<i32, string>"),
  });
  semanticProgram.publishedRoutingLookups.bindingFactIndicesByExpr.insert_or_assign(7501, 0);

  primec::Expr bindingExpr = primec::validation_test_support::makeBindingNameExpr("selected");
  bindingExpr.semanticNodeId = 7501;

  primec::Expr initExpr;
  initExpr.kind = primec::Expr::Kind::Name;
  initExpr.name = "selected";

  const auto semanticIndex =
      primec::ir_lowerer::buildSemanticProductIndex(&semanticProgram);
  const primec::ir_lowerer::StatementBindingTypeInfo info =
      primec::ir_lowerer::inferStatementBindingTypeInfo(
          bindingExpr,
          initExpr,
          {},
          [](const primec::Expr &) { return false; },
          [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::Kind::Value; },
          [](const primec::Expr &, primec::ir_lowerer::LocalInfo::Kind) {
            return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
          },
          [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
            return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
          },
          {},
          &semanticProgram,
          &semanticIndex);

  CHECK(info.kind == primec::ir_lowerer::LocalInfo::Kind::Value);
  CHECK(info.keyValueKeyKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
  CHECK(info.keyValueValueKind == primec::ir_lowerer::LocalInfo::ValueKind::String);
}

TEST_CASE("ir lowerer statement binding helper prefers semantic initializer binding facts") {
  primec::SemanticProgram semanticProgram;
  semanticProgram.bindingFacts.push_back(primec::SemanticProgramBindingFact{
      .scopePath = "/main",
      .siteKind = "local",
      .name = "source",
      .bindingTypeText = "",
      .isMutable = false,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .sourceLine = 22,
      .sourceColumn = 5,
      .semanticNodeId = 7601,
      .resolvedPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/source"),
      .bindingTypeTextId =
          primec::semanticProgramInternCallTargetString(
              semanticProgram, "map<i32, string>"),
  });
  semanticProgram.publishedRoutingLookups.bindingFactIndicesByExpr.insert_or_assign(7601, 0);

  primec::Expr bindingExpr = primec::validation_test_support::makeBindingNameExpr("selected");

  primec::Expr initExpr;
  initExpr.kind = primec::Expr::Kind::Name;
  initExpr.name = "source";
  initExpr.semanticNodeId = 7601;

  primec::ir_lowerer::LocalInfo staleLocalInfo;
  staleLocalInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  staleLocalInfo.keyValueKeyKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  staleLocalInfo.keyValueValueKind = primec::ir_lowerer::LocalInfo::ValueKind::Bool;
  staleLocalInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Bool;
  staleLocalInfo.structTypeName = "/stale/Map";
  const primec::ir_lowerer::LocalMap locals{{"source", staleLocalInfo}};

  const auto semanticIndex =
      primec::ir_lowerer::buildSemanticProductIndex(&semanticProgram);
  const primec::ir_lowerer::StatementBindingTypeInfo info =
      primec::ir_lowerer::inferStatementBindingTypeInfo(
          bindingExpr,
          initExpr,
          locals,
          [](const primec::Expr &) { return false; },
          [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::Kind::Value; },
          [](const primec::Expr &, primec::ir_lowerer::LocalInfo::Kind) {
            return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
          },
          [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
            return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
          },
          {},
          &semanticProgram,
          &semanticIndex);

  CHECK(info.kind == primec::ir_lowerer::LocalInfo::Kind::Value);
  CHECK(info.keyValueKeyKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
  CHECK(info.keyValueValueKind == primec::ir_lowerer::LocalInfo::ValueKind::String);
  CHECK(info.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::String);
  CHECK(info.structTypeName != "/stale/Map");
}

TEST_CASE("ir lowerer semantic-product adapter ignores query resolved-path fallback") {
  primec::Expr queryExpr;
  queryExpr.kind = primec::Expr::Kind::Call;
  queryExpr.name = "lookup";
  queryExpr.semanticNodeId = 8101;

  primec::SemanticProgram semanticProgram;
  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      .scopePath = "/main",
      .callName = "lookup",
      .sourceLine = 15,
      .sourceColumn = 5,
      .semanticNodeId = 8101,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/lookup"),
      .stdlibSurfaceId = std::nullopt,
  });
  const primec::SymbolId callNameId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "lookup");
  const primec::SymbolId resolvedPathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/lookup");
  semanticProgram.queryFacts.push_back(primec::SemanticProgramQueryFact{
      .scopePath = "/main",
      .callName = "lookup",
      .queryTypeText = "Result<i32, FileError>",
      .bindingTypeText = "Result<i32, FileError>",
      .receiverBindingTypeText = "",
      .hasResultType = true,
      .resultTypeHasValue = true,
      .resultValueType = "i32",
      .resultErrorType = "FileError",
      .sourceLine = 15,
      .sourceColumn = 5,
      .semanticNodeId = 0,
      .callNameId = callNameId,
      .resolvedPathId = resolvedPathId,
  });
  semanticProgram.publishedRoutingLookups.queryFactIndicesByResolvedPathAndCallNameId
      .insert_or_assign((static_cast<uint64_t>(resolvedPathId) << 32) |
                            static_cast<uint64_t>(callNameId),
                        0);

  const auto adapter = primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);
  CHECK(adapter.semanticIndex.queryFactsByExpr.empty());
  const auto *queryFact = primec::ir_lowerer::findSemanticProductQueryFact(adapter, queryExpr);
  CHECK(queryFact == nullptr);
}

TEST_CASE("ir lowerer semantic-product adapter uses query semantic-id matches without path fallback") {
  primec::Expr queryExpr;
  queryExpr.kind = primec::Expr::Kind::Call;
  queryExpr.name = "lookup";
  queryExpr.semanticNodeId = 8202;

  primec::SemanticProgram semanticProgram;
  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      .scopePath = "/main",
      .callName = "lookup",
      .sourceLine = 17,
      .sourceColumn = 5,
      .semanticNodeId = 8202,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/lookup"),
      .stdlibSurfaceId = std::nullopt,
  });

  const primec::SymbolId callNameId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "lookup");
  const primec::SymbolId resolvedPathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/lookup");

  semanticProgram.queryFacts.push_back(primec::SemanticProgramQueryFact{
      .scopePath = "/main",
      .callName = "lookup",
      .queryTypeText = "Result<i32, FileError>",
      .bindingTypeText = "Result<i32, FileError>",
      .receiverBindingTypeText = "",
      .hasResultType = true,
      .resultTypeHasValue = true,
      .resultValueType = "i32",
      .resultErrorType = "FileError",
      .sourceLine = 17,
      .sourceColumn = 5,
      .semanticNodeId = 8202,
      .callNameId = callNameId,
      .resolvedPathId = resolvedPathId,
  });
  semanticProgram.queryFacts.push_back(primec::SemanticProgramQueryFact{
      .scopePath = "/main",
      .callName = "lookup",
      .queryTypeText = "Result<f64, FileError>",
      .bindingTypeText = "Result<f64, FileError>",
      .receiverBindingTypeText = "",
      .hasResultType = true,
      .resultTypeHasValue = true,
      .resultValueType = "f64",
      .resultErrorType = "FileError",
      .sourceLine = 17,
      .sourceColumn = 5,
      .semanticNodeId = 0,
      .callNameId = callNameId,
      .resolvedPathId = resolvedPathId,
  });
  semanticProgram.publishedRoutingLookups.queryFactIndicesByExpr.insert_or_assign(8202, 0);
  semanticProgram.publishedRoutingLookups.queryFactIndicesByResolvedPathAndCallNameId
      .insert_or_assign((static_cast<uint64_t>(resolvedPathId) << 32) |
                            static_cast<uint64_t>(callNameId),
                        1);

  const auto adapter = primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);
  const auto *queryFact = primec::ir_lowerer::findSemanticProductQueryFact(adapter, queryExpr);
  REQUIRE(queryFact != nullptr);
  CHECK(queryFact->semanticNodeId == 8202);
  CHECK(queryFact->queryTypeText == "Result<i32, FileError>");
}

TEST_CASE("ir lowerer semantic-product adapter uses published query semantic-id without path fallback") {
  primec::Expr queryExpr;
  queryExpr.kind = primec::Expr::Kind::Call;
  queryExpr.name = "lookup";
  queryExpr.sourceLine = 19;
  queryExpr.sourceColumn = 5;
  queryExpr.semanticNodeId = 8303;

  primec::SemanticProgram semanticProgram;
  const primec::SymbolId callNameId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "lookup");
  const primec::SymbolId resolvedPathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/lookup");
  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      .scopePath = "/main",
      .callName = "lookup",
      .sourceLine = 19,
      .sourceColumn = 5,
      .semanticNodeId = 8303,
      .resolvedPathId = resolvedPathId,
      .stdlibSurfaceId = std::nullopt,
  });
  semanticProgram.queryFacts.push_back(primec::SemanticProgramQueryFact{
      .scopePath = "/main",
      .callName = "lookup",
      .queryTypeText = "Result<i32, FileError>",
      .bindingTypeText = "Result<i32, FileError>",
      .receiverBindingTypeText = "",
      .hasResultType = true,
      .resultTypeHasValue = true,
      .resultValueType = "i32",
      .resultErrorType = "FileError",
      .sourceLine = 19,
      .sourceColumn = 5,
      .semanticNodeId = 8303,
      .callNameId = callNameId,
      .resolvedPathId = resolvedPathId,
  });
  semanticProgram.publishedRoutingLookups.queryFactIndicesByExpr.insert_or_assign(8303, 0);
  semanticProgram.queryFacts.push_back(primec::SemanticProgramQueryFact{
      .scopePath = "/main",
      .callName = "lookup",
      .queryTypeText = "Result<f64, FileError>",
      .bindingTypeText = "Result<f64, FileError>",
      .receiverBindingTypeText = "",
      .hasResultType = true,
      .resultTypeHasValue = true,
      .resultValueType = "f64",
      .resultErrorType = "FileError",
      .sourceLine = 19,
      .sourceColumn = 5,
      .semanticNodeId = 0,
      .callNameId = callNameId,
      .resolvedPathId = resolvedPathId,
  });
  semanticProgram.publishedRoutingLookups.queryFactIndicesByResolvedPathAndCallNameId
      .insert_or_assign((static_cast<uint64_t>(resolvedPathId) << 32) |
                            static_cast<uint64_t>(callNameId),
                        1);

  const auto adapter = primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);
  CHECK(adapter.semanticIndex.queryFactsByExpr.count(8303) == 1);
  const auto *queryFact = primec::ir_lowerer::findSemanticProductQueryFact(adapter, queryExpr);
  REQUIRE(queryFact != nullptr);
  CHECK(queryFact->semanticNodeId == 8303);
  CHECK(queryFact->queryTypeText == "Result<i32, FileError>");
}

TEST_CASE("ir lowerer semantic-product index does not expose query path fallback") {
  primec::Expr queryExpr;
  queryExpr.kind = primec::Expr::Kind::Call;
  queryExpr.name = "lookup";
  queryExpr.semanticNodeId = 8101;

  primec::SemanticProgram semanticProgram;
  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      .scopePath = "/main",
      .callName = "lookup",
      .sourceLine = 15,
      .sourceColumn = 5,
      .semanticNodeId = 8101,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/lookup"),
      .stdlibSurfaceId = std::nullopt,
  });
  const primec::SymbolId callNameId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "lookup");
  const primec::SymbolId resolvedPathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/lookup");
  semanticProgram.queryFacts.push_back(primec::SemanticProgramQueryFact{
      .scopePath = "/main",
      .callName = "lookup",
      .queryTypeText = "Result<i32, FileError>",
      .bindingTypeText = "Result<i32, FileError>",
      .receiverBindingTypeText = "",
      .hasResultType = true,
      .resultTypeHasValue = true,
      .resultValueType = "i32",
      .resultErrorType = "FileError",
      .sourceLine = 15,
      .sourceColumn = 5,
      .semanticNodeId = 0,
      .callNameId = callNameId,
      .resolvedPathId = resolvedPathId,
  });

  const auto semanticIndex =
      primec::ir_lowerer::buildSemanticProductIndex(&semanticProgram);
  CHECK(semanticIndex.queryFactsByExpr.empty());
  const auto *queryFact =
      primec::ir_lowerer::findSemanticProductQueryFact(&semanticProgram, semanticIndex, queryExpr);
  CHECK(queryFact == nullptr);
}

TEST_CASE("ir lowerer semantic-product index ignores raw query facts without published lookup") {
  primec::Expr queryExpr;
  queryExpr.kind = primec::Expr::Kind::Call;
  queryExpr.name = "lookup";
  queryExpr.semanticNodeId = 8102;

  primec::SemanticProgram semanticProgram;
  semanticProgram.queryFacts.push_back(primec::SemanticProgramQueryFact{
      .scopePath = "/main",
      .callName = "lookup",
      .queryTypeText = "Result<i32, FileError>",
      .bindingTypeText = "Result<i32, FileError>",
      .receiverBindingTypeText = "",
      .hasResultType = true,
      .resultTypeHasValue = true,
      .resultValueType = "i32",
      .resultErrorType = "FileError",
      .sourceLine = 16,
      .sourceColumn = 5,
      .semanticNodeId = 8102,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/lookup"),
  });

  const auto semanticIndex =
      primec::ir_lowerer::buildSemanticProductIndex(&semanticProgram);
  CHECK(semanticIndex.queryFactsByExpr.empty());
  const auto *queryFact =
      primec::ir_lowerer::findSemanticProductQueryFact(&semanticProgram, semanticIndex, queryExpr);
  CHECK(queryFact == nullptr);
}

TEST_CASE("ir lowerer semantic-product adapter ignores try operand-path fallback") {
  primec::Expr operandExpr;
  operandExpr.kind = primec::Expr::Kind::Call;
  operandExpr.name = "lookup";
  operandExpr.semanticNodeId = 9101;

  primec::Expr tryExpr;
  tryExpr.kind = primec::Expr::Kind::Call;
  tryExpr.name = "try";
  tryExpr.args = {operandExpr};
  tryExpr.semanticNodeId = 0;
  tryExpr.sourceLine = 33;
  tryExpr.sourceColumn = 9;

  primec::SemanticProgram semanticProgram;
  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      .scopePath = "/main",
      .callName = "lookup",
      .sourceLine = 33,
      .sourceColumn = 9,
      .semanticNodeId = 9101,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/lookup"),
      .stdlibSurfaceId = std::nullopt,
  });
  const primec::SymbolId operandResolvedPathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/lookup");
  semanticProgram.tryFacts.push_back(primec::SemanticProgramTryFact{
      .scopePath = "/main",
      .operandBindingTypeText = "Result<i32, FileError>",
      .operandReceiverBindingTypeText = "",
      .operandQueryTypeText = "Result<i32, FileError>",
      .valueType = "i32",
      .errorType = "FileError",
      .contextReturnKind = "return",
      .onErrorHandlerPath = "/handler",
      .onErrorErrorType = "FileError",
      .onErrorBoundArgCount = 1,
      .sourceLine = 33,
      .sourceColumn = 9,
      .semanticNodeId = 0,
      .operandResolvedPathId = operandResolvedPathId,
  });
  const uint64_t tryKey =
      (static_cast<uint64_t>(operandResolvedPathId) << 32) ^
      (static_cast<uint64_t>(static_cast<uint32_t>(tryExpr.sourceLine)) * 1315423911ULL) ^
      static_cast<uint64_t>(static_cast<uint32_t>(tryExpr.sourceColumn));
  semanticProgram.publishedRoutingLookups.tryFactIndicesByOperandPathAndSource.insert_or_assign(
      tryKey, 0);

  const auto adapter = primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);
  CHECK(adapter.semanticIndex.tryFactsByExpr.empty());
  const auto *tryFact = primec::ir_lowerer::findSemanticProductTryFact(adapter, tryExpr);
  CHECK(tryFact == nullptr);
}

TEST_CASE("ir lowerer semantic-product adapter uses try semantic-id matches without path fallback") {
  primec::Expr operandExpr;
  operandExpr.kind = primec::Expr::Kind::Call;
  operandExpr.name = "lookup";
  operandExpr.semanticNodeId = 9201;

  primec::Expr tryExpr;
  tryExpr.kind = primec::Expr::Kind::Call;
  tryExpr.name = "try";
  tryExpr.args = {operandExpr};
  tryExpr.semanticNodeId = 9202;
  tryExpr.sourceLine = 41;
  tryExpr.sourceColumn = 11;

  primec::SemanticProgram semanticProgram;
  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      .scopePath = "/main",
      .callName = "lookup",
      .sourceLine = 41,
      .sourceColumn = 11,
      .semanticNodeId = 9201,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/lookup"),
      .stdlibSurfaceId = std::nullopt,
  });

  const primec::SymbolId operandResolvedPathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/lookup");

  semanticProgram.tryFacts.push_back(primec::SemanticProgramTryFact{
      .scopePath = "/main",
      .operandBindingTypeText = "Result<i32, FileError>",
      .operandReceiverBindingTypeText = "",
      .operandQueryTypeText = "Result<i32, FileError>",
      .valueType = "i32",
      .errorType = "FileError",
      .contextReturnKind = "return",
      .onErrorHandlerPath = "/handler",
      .onErrorErrorType = "FileError",
      .onErrorBoundArgCount = 1,
      .sourceLine = 41,
      .sourceColumn = 11,
      .semanticNodeId = 9202,
      .operandResolvedPathId = operandResolvedPathId,
  });
  semanticProgram.tryFacts.push_back(primec::SemanticProgramTryFact{
      .scopePath = "/main",
      .operandBindingTypeText = "Result<f64, FileError>",
      .operandReceiverBindingTypeText = "",
      .operandQueryTypeText = "Result<f64, FileError>",
      .valueType = "f64",
      .errorType = "FileError",
      .contextReturnKind = "return",
      .onErrorHandlerPath = "/handler",
      .onErrorErrorType = "FileError",
      .onErrorBoundArgCount = 1,
      .sourceLine = 41,
      .sourceColumn = 11,
      .semanticNodeId = 0,
      .operandResolvedPathId = operandResolvedPathId,
  });
  const uint64_t tryKey =
      (static_cast<uint64_t>(operandResolvedPathId) << 32) ^
      (static_cast<uint64_t>(static_cast<uint32_t>(tryExpr.sourceLine)) * 1315423911ULL) ^
      static_cast<uint64_t>(static_cast<uint32_t>(tryExpr.sourceColumn));
  semanticProgram.publishedRoutingLookups.tryFactIndicesByExpr.insert_or_assign(9202, 0);
  semanticProgram.publishedRoutingLookups.tryFactIndicesByOperandPathAndSource.insert_or_assign(
      tryKey, 1);

  const auto adapter = primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);
  const auto *tryFact = primec::ir_lowerer::findSemanticProductTryFact(adapter, tryExpr);
  REQUIRE(tryFact != nullptr);
  CHECK(tryFact->semanticNodeId == 9202);
  CHECK(tryFact->valueType == "i32");
}

TEST_CASE("ir lowerer semantic-product index ignores raw try facts without published lookup") {
  primec::Expr tryExpr;
  tryExpr.kind = primec::Expr::Kind::Call;
  tryExpr.name = "try";
  tryExpr.semanticNodeId = 9203;
  tryExpr.sourceLine = 42;
  tryExpr.sourceColumn = 13;

  primec::SemanticProgram semanticProgram;
  semanticProgram.tryFacts.push_back(primec::SemanticProgramTryFact{
      .scopePath = "/main",
      .operandBindingTypeText = "Result<i32, FileError>",
      .operandReceiverBindingTypeText = "",
      .operandQueryTypeText = "Result<i32, FileError>",
      .valueType = "i32",
      .errorType = "FileError",
      .contextReturnKind = "return",
      .onErrorHandlerPath = "/handler",
      .onErrorErrorType = "FileError",
      .onErrorBoundArgCount = 1,
      .sourceLine = 42,
      .sourceColumn = 13,
      .semanticNodeId = 9203,
      .operandResolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/lookup"),
  });

  const auto semanticIndex =
      primec::ir_lowerer::buildSemanticProductIndex(&semanticProgram);
  CHECK(semanticIndex.tryFactsByExpr.empty());
  const auto *tryFact =
      primec::ir_lowerer::findSemanticProductTryFact(&semanticProgram, semanticIndex, tryExpr);
  CHECK(tryFact == nullptr);
}

TEST_CASE("ir lowerer semantic-product index does not expose try operand-path fallback") {
  primec::Expr operandExpr;
  operandExpr.kind = primec::Expr::Kind::Call;
  operandExpr.name = "lookup";
  operandExpr.semanticNodeId = 9101;

  primec::Expr tryExpr;
  tryExpr.kind = primec::Expr::Kind::Call;
  tryExpr.name = "try";
  tryExpr.args = {operandExpr};
  tryExpr.semanticNodeId = 0;
  tryExpr.sourceLine = 33;
  tryExpr.sourceColumn = 9;

  primec::SemanticProgram semanticProgram;
  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      .scopePath = "/main",
      .callName = "lookup",
      .sourceLine = 33,
      .sourceColumn = 9,
      .semanticNodeId = 9101,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/lookup"),
      .stdlibSurfaceId = std::nullopt,
  });
  const primec::SymbolId operandResolvedPathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/lookup");
  semanticProgram.tryFacts.push_back(primec::SemanticProgramTryFact{
      .scopePath = "/main",
      .operandBindingTypeText = "Result<i32, FileError>",
      .operandReceiverBindingTypeText = "",
      .operandQueryTypeText = "Result<i32, FileError>",
      .valueType = "i32",
      .errorType = "FileError",
      .contextReturnKind = "return",
      .onErrorHandlerPath = "/handler",
      .onErrorErrorType = "FileError",
      .onErrorBoundArgCount = 1,
      .sourceLine = 33,
      .sourceColumn = 9,
      .semanticNodeId = 0,
      .operandResolvedPathId = operandResolvedPathId,
  });

  const auto semanticIndex =
      primec::ir_lowerer::buildSemanticProductIndex(&semanticProgram);
  CHECK(semanticIndex.tryFactsByExpr.empty());
  const auto *tryFact =
      primec::ir_lowerer::findSemanticProductTryFact(&semanticProgram, semanticIndex, tryExpr);
  CHECK(tryFact == nullptr);
}

TEST_CASE("ir lowerer call helpers keep slashless map import aliases raw") {
  primec::Definition canonicalMapCountDef;
  canonicalMapCountDef.fullPath = "/std/collections/map/count";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/std/collections/map/count", &canonicalMapCountDef},
  };
  const std::unordered_map<std::string, std::string> importAliases = {
      {"count_alias", "std/collections/map/count"},
  };
  const auto resolveExprPath = primec::ir_lowerer::makeResolveCallPathFromScope(defMap, importAliases);

  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "count_alias";
  CHECK(primec::ir_lowerer::resolveDefinitionCall(callExpr, defMap, resolveExprPath) == nullptr);
}

TEST_CASE("ir lowerer call helpers keep explicit canonical map contains and tryAt same-path defs") {
  primec::Definition canonicalMapContainsDef;
  canonicalMapContainsDef.fullPath = "/std/collections/map/contains";
  primec::Definition canonicalMapTryAtDef;
  canonicalMapTryAtDef.fullPath = "/std/collections/map/tryAt";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {canonicalMapContainsDef.fullPath, &canonicalMapContainsDef},
      {canonicalMapTryAtDef.fullPath, &canonicalMapTryAtDef},
  };
  const auto resolveExprPath = [](const primec::Expr &expr) { return expr.name; };

  primec::Expr containsCall;
  containsCall.kind = primec::Expr::Kind::Call;
  containsCall.name = "/std/collections/map/contains";
  primec::Expr valuesArg;
  valuesArg.kind = primec::Expr::Kind::Name;
  valuesArg.name = "values";
  primec::Expr keyArg;
  keyArg.kind = primec::Expr::Kind::Literal;
  keyArg.intWidth = 32;
  keyArg.literalValue = 1;
  containsCall.args = {valuesArg, keyArg};

  primec::Expr tryAtCall = containsCall;
  tryAtCall.name = "/std/collections/map/tryAt";

  CHECK(primec::ir_lowerer::resolveDefinitionCall(containsCall, defMap, resolveExprPath) ==
        &canonicalMapContainsDef);
  CHECK(primec::ir_lowerer::resolveDefinitionCall(tryAtCall, defMap, resolveExprPath) ==
        &canonicalMapTryAtDef);
}

TEST_CASE("ir lowerer call helpers keep explicit map helper same-path defs") {
  primec::Definition canonicalMapCountDef;
  canonicalMapCountDef.fullPath = "/std/collections/map/count";
  primec::Definition canonicalMapAtDef;
  canonicalMapAtDef.fullPath = "/std/collections/map/at";
  primec::Definition canonicalMapAtUnsafeDef;
  canonicalMapAtUnsafeDef.fullPath = "/std/collections/map/at_unsafe";
  primec::Definition aliasMapCountDef;
  aliasMapCountDef.fullPath = "/map/count";
  primec::Definition aliasMapContainsDef;
  aliasMapContainsDef.fullPath = "/map/contains";
  primec::Definition aliasMapTryAtDef;
  aliasMapTryAtDef.fullPath = "/map/tryAt";
  primec::Definition aliasMapAtDef;
  aliasMapAtDef.fullPath = "/map/at";
  primec::Definition aliasMapAtUnsafeDef;
  aliasMapAtUnsafeDef.fullPath = "/map/at_unsafe";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {canonicalMapCountDef.fullPath, &canonicalMapCountDef},
      {canonicalMapAtDef.fullPath, &canonicalMapAtDef},
      {canonicalMapAtUnsafeDef.fullPath, &canonicalMapAtUnsafeDef},
      {aliasMapCountDef.fullPath, &aliasMapCountDef},
      {aliasMapContainsDef.fullPath, &aliasMapContainsDef},
      {aliasMapTryAtDef.fullPath, &aliasMapTryAtDef},
      {aliasMapAtDef.fullPath, &aliasMapAtDef},
      {aliasMapAtUnsafeDef.fullPath, &aliasMapAtUnsafeDef},
  };
  const auto resolveExprPath = [](const primec::Expr &expr) { return expr.name; };
  const auto resolveExprPathWithCanonicalAliasAccessFallback =
      [](const primec::Expr &expr) {
        if (expr.name == "/map/at") {
          return std::string("/std/collections/map/at");
        }
        if (expr.name == "/map/at_unsafe") {
          return std::string("/std/collections/map/at_unsafe");
        }
        return expr.name;
      };

  primec::Expr valuesArg;
  valuesArg.kind = primec::Expr::Kind::Name;
  valuesArg.name = "values";
  primec::Expr keyArg;
  keyArg.kind = primec::Expr::Kind::Literal;
  keyArg.intWidth = 32;
  keyArg.literalValue = 1;

  primec::Expr canonicalCountCall;
  canonicalCountCall.kind = primec::Expr::Kind::Call;
  canonicalCountCall.name = "/std/collections/map/count";
  canonicalCountCall.args = {valuesArg};

  primec::Expr canonicalAtCall;
  canonicalAtCall.kind = primec::Expr::Kind::Call;
  canonicalAtCall.name = "/std/collections/map/at";
  canonicalAtCall.args = {valuesArg, keyArg};

  primec::Expr aliasCountCall = canonicalCountCall;
  aliasCountCall.name = "/map/count";

  primec::Expr aliasContainsCall = canonicalAtCall;
  aliasContainsCall.name = "/map/contains";

  primec::Expr aliasTryAtCall = canonicalAtCall;
  aliasTryAtCall.name = "/map/tryAt";

  primec::Expr aliasAtCall = canonicalAtCall;
  aliasAtCall.name = "/map/at";

  primec::Expr canonicalAtUnsafeCall = canonicalAtCall;
  canonicalAtUnsafeCall.name = "/std/collections/map/at_unsafe";

  primec::Expr aliasAtUnsafeCall = canonicalAtUnsafeCall;
  aliasAtUnsafeCall.name = "/map/at_unsafe";

  CHECK(primec::ir_lowerer::resolveDefinitionCall(canonicalCountCall, defMap, resolveExprPath) ==
        &canonicalMapCountDef);
  CHECK(primec::ir_lowerer::resolveDefinitionCall(canonicalAtCall, defMap, resolveExprPath) ==
        &canonicalMapAtDef);
  CHECK(primec::ir_lowerer::resolveDefinitionCall(canonicalAtUnsafeCall, defMap, resolveExprPath) ==
        &canonicalMapAtUnsafeDef);
  CHECK(primec::ir_lowerer::resolveDefinitionCall(aliasCountCall, defMap, resolveExprPath) ==
        &aliasMapCountDef);
  CHECK(primec::ir_lowerer::resolveDefinitionCall(aliasContainsCall, defMap, resolveExprPath) ==
        &aliasMapContainsDef);
  CHECK(primec::ir_lowerer::resolveDefinitionCall(aliasTryAtCall, defMap, resolveExprPath) ==
        &aliasMapTryAtDef);
  CHECK(primec::ir_lowerer::resolveDefinitionCall(aliasAtCall, defMap, resolveExprPath) ==
        &aliasMapAtDef);
  CHECK(primec::ir_lowerer::resolveDefinitionCall(aliasAtUnsafeCall, defMap, resolveExprPath) ==
        &aliasMapAtUnsafeDef);
  CHECK(primec::ir_lowerer::resolveDefinitionCall(
            aliasAtCall, defMap, resolveExprPathWithCanonicalAliasAccessFallback) ==
        &canonicalMapAtDef);
  CHECK(primec::ir_lowerer::resolveDefinitionCall(
            aliasAtUnsafeCall, defMap, resolveExprPathWithCanonicalAliasAccessFallback) ==
        &canonicalMapAtUnsafeDef);
}

TEST_CASE("ir lowerer call helpers keep bare semantic map sugar on canonical defs") {
  primec::Definition canonicalMapCountDef;
  canonicalMapCountDef.fullPath = "/std/collections/map/count";
  primec::Definition canonicalMapContainsDef;
  canonicalMapContainsDef.fullPath = "/std/collections/map/contains";
  primec::Definition canonicalMapTryAtDef;
  canonicalMapTryAtDef.fullPath = "/std/collections/map/tryAt";
  primec::Definition canonicalMapAtDef;
  canonicalMapAtDef.fullPath = "/std/collections/map/at";
  primec::Definition canonicalMapAtUnsafeDef;
  canonicalMapAtUnsafeDef.fullPath = "/std/collections/map/at_unsafe";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {canonicalMapCountDef.fullPath, &canonicalMapCountDef},
      {canonicalMapContainsDef.fullPath, &canonicalMapContainsDef},
      {canonicalMapTryAtDef.fullPath, &canonicalMapTryAtDef},
      {canonicalMapAtDef.fullPath, &canonicalMapAtDef},
      {canonicalMapAtUnsafeDef.fullPath, &canonicalMapAtUnsafeDef},
  };
  const auto resolveExprPath = [](const primec::Expr &expr) {
    if (expr.name == "count") {
      return std::string("/std/collections/map/count");
    }
    if (expr.name == "contains") {
      return std::string("/std/collections/map/contains");
    }
    if (expr.name == "tryAt") {
      return std::string("/std/collections/map/tryAt");
    }
    if (expr.name == "at") {
      return std::string("/std/collections/map/at");
    }
    if (expr.name == "at_unsafe") {
      return std::string("/std/collections/map/at_unsafe");
    }
    return expr.name;
  };

  primec::Expr valuesArg;
  valuesArg.kind = primec::Expr::Kind::Name;
  valuesArg.name = "values";
  primec::Expr keyArg;
  keyArg.kind = primec::Expr::Kind::Literal;
  keyArg.intWidth = 32;
  keyArg.literalValue = 1;

  primec::Expr countCall;
  countCall.kind = primec::Expr::Kind::Call;
  countCall.name = "count";
  countCall.semanticNodeId = 91;
  countCall.args = {valuesArg};

  primec::Expr containsCall;
  containsCall.kind = primec::Expr::Kind::Call;
  containsCall.name = "contains";
  containsCall.semanticNodeId = 92;
  containsCall.args = {valuesArg, keyArg};

  primec::Expr tryAtCall = containsCall;
  tryAtCall.name = "tryAt";
  tryAtCall.semanticNodeId = 93;

  primec::Expr atCall;
  atCall.kind = primec::Expr::Kind::Call;
  atCall.name = "at";
  atCall.semanticNodeId = 94;
  atCall.args = {valuesArg, keyArg};

  primec::Expr atUnsafeCall = atCall;
  atUnsafeCall.name = "at_unsafe";
  atUnsafeCall.semanticNodeId = 95;

  CHECK(primec::ir_lowerer::resolveDefinitionCall(countCall, defMap, resolveExprPath) ==
        &canonicalMapCountDef);
  CHECK(primec::ir_lowerer::resolveDefinitionCall(containsCall, defMap, resolveExprPath) ==
        &canonicalMapContainsDef);
  CHECK(primec::ir_lowerer::resolveDefinitionCall(tryAtCall, defMap, resolveExprPath) ==
        &canonicalMapTryAtDef);
  CHECK(primec::ir_lowerer::resolveDefinitionCall(atCall, defMap, resolveExprPath) ==
        &canonicalMapAtDef);
  CHECK(primec::ir_lowerer::resolveDefinitionCall(atUnsafeCall, defMap, resolveExprPath) ==
        &canonicalMapAtUnsafeDef);
}

TEST_CASE("ir lowerer call helpers resolve bare non-semantic contains and tryAt canonical defs") {
  primec::Definition canonicalMapContainsDef;
  canonicalMapContainsDef.fullPath = "/std/collections/map/contains";
  primec::Definition canonicalMapTryAtDef;
  canonicalMapTryAtDef.fullPath = "/std/collections/map/tryAt";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {canonicalMapContainsDef.fullPath, &canonicalMapContainsDef},
      {canonicalMapTryAtDef.fullPath, &canonicalMapTryAtDef},
  };
  const auto resolveExprPath = [](const primec::Expr &expr) {
    if (expr.name == "contains") {
      return std::string("/std/collections/map/contains");
    }
    if (expr.name == "tryAt") {
      return std::string("/std/collections/map/tryAt");
    }
    return expr.name;
  };

  primec::Expr valuesArg;
  valuesArg.kind = primec::Expr::Kind::Name;
  valuesArg.name = "values";
  primec::Expr keyArg;
  keyArg.kind = primec::Expr::Kind::Literal;
  keyArg.intWidth = 32;
  keyArg.literalValue = 1;

  primec::Expr containsCall;
  containsCall.kind = primec::Expr::Kind::Call;
  containsCall.name = "contains";
  containsCall.args = {valuesArg, keyArg};

  primec::Expr tryAtCall = containsCall;
  tryAtCall.name = "tryAt";

  CHECK(primec::ir_lowerer::resolveDefinitionCall(containsCall, defMap, resolveExprPath) ==
        &canonicalMapContainsDef);
  CHECK(primec::ir_lowerer::resolveDefinitionCall(tryAtCall, defMap, resolveExprPath) ==
        &canonicalMapTryAtDef);
}

TEST_CASE("ir lowerer call helpers prefer canonical remaps over rooted map alias defs") {
  primec::Definition canonicalMapCountDef;
  canonicalMapCountDef.fullPath = "/std/collections/map/count";
  primec::Definition canonicalMapContainsDef;
  canonicalMapContainsDef.fullPath = "/std/collections/map/contains";
  primec::Definition canonicalMapTryAtDef;
  canonicalMapTryAtDef.fullPath = "/std/collections/map/tryAt";
  primec::Definition aliasMapCountDef;
  aliasMapCountDef.fullPath = "/map/count";
  primec::Definition aliasMapContainsDef;
  aliasMapContainsDef.fullPath = "/map/contains";
  primec::Definition aliasMapTryAtDef;
  aliasMapTryAtDef.fullPath = "/map/tryAt";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {canonicalMapCountDef.fullPath, &canonicalMapCountDef},
      {canonicalMapContainsDef.fullPath, &canonicalMapContainsDef},
      {canonicalMapTryAtDef.fullPath, &canonicalMapTryAtDef},
      {aliasMapCountDef.fullPath, &aliasMapCountDef},
      {aliasMapContainsDef.fullPath, &aliasMapContainsDef},
      {aliasMapTryAtDef.fullPath, &aliasMapTryAtDef},
  };
  const auto resolveExprPath = [](const primec::Expr &expr) {
    if (expr.name == "/map/count") {
      return std::string("/std/collections/map/count");
    }
    if (expr.name == "/map/contains") {
      return std::string("/std/collections/map/contains");
    }
    if (expr.name == "/map/tryAt") {
      return std::string("/std/collections/map/tryAt");
    }
    return expr.name;
  };

  primec::Expr valuesArg;
  valuesArg.kind = primec::Expr::Kind::Name;
  valuesArg.name = "values";
  primec::Expr keyArg;
  keyArg.kind = primec::Expr::Kind::Literal;
  keyArg.intWidth = 32;
  keyArg.literalValue = 1;

  primec::Expr aliasCountCall;
  aliasCountCall.kind = primec::Expr::Kind::Call;
  aliasCountCall.name = "/map/count";
  aliasCountCall.args = {valuesArg};

  primec::Expr aliasContainsCall;
  aliasContainsCall.kind = primec::Expr::Kind::Call;
  aliasContainsCall.name = "/map/contains";
  aliasContainsCall.args = {valuesArg, keyArg};

  primec::Expr aliasTryAtCall = aliasContainsCall;
  aliasTryAtCall.name = "/map/tryAt";

  CHECK(primec::ir_lowerer::resolveDefinitionCall(aliasCountCall, defMap, resolveExprPath) ==
        &canonicalMapCountDef);
  CHECK(primec::ir_lowerer::resolveDefinitionCall(aliasContainsCall, defMap, resolveExprPath) ==
        &canonicalMapContainsDef);
  CHECK(primec::ir_lowerer::resolveDefinitionCall(aliasTryAtCall, defMap, resolveExprPath) ==
        &canonicalMapTryAtDef);
}

TEST_CASE("ir lowerer call helpers reject rooted map alias def families") {
  primec::Definition aliasMapCountDef;
  aliasMapCountDef.fullPath = "/map/count__ov0";
  primec::Definition aliasMapContainsDef;
  aliasMapContainsDef.fullPath = "/map/contains__tmono";
  primec::Definition aliasMapTryAtDef;
  aliasMapTryAtDef.fullPath = "/map/tryAt__ov1";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {aliasMapCountDef.fullPath, &aliasMapCountDef},
      {aliasMapContainsDef.fullPath, &aliasMapContainsDef},
      {aliasMapTryAtDef.fullPath, &aliasMapTryAtDef},
  };
  const auto resolveExprPath = [](const primec::Expr &expr) { return expr.name; };

  primec::Expr valuesArg;
  valuesArg.kind = primec::Expr::Kind::Name;
  valuesArg.name = "values";
  primec::Expr keyArg;
  keyArg.kind = primec::Expr::Kind::Literal;
  keyArg.intWidth = 32;
  keyArg.literalValue = 1;

  primec::Expr aliasCountCall;
  aliasCountCall.kind = primec::Expr::Kind::Call;
  aliasCountCall.name = "/map/count";
  aliasCountCall.args = {valuesArg};

  primec::Expr aliasContainsCall;
  aliasContainsCall.kind = primec::Expr::Kind::Call;
  aliasContainsCall.name = "/map/contains";
  aliasContainsCall.args = {valuesArg, keyArg};

  primec::Expr aliasTryAtCall = aliasContainsCall;
  aliasTryAtCall.name = "/map/tryAt";

  CHECK(primec::ir_lowerer::resolveDefinitionCall(aliasCountCall, defMap, resolveExprPath) ==
        nullptr);
  CHECK(primec::ir_lowerer::resolveDefinitionCall(aliasContainsCall, defMap, resolveExprPath) ==
        nullptr);
  CHECK(primec::ir_lowerer::resolveDefinitionCall(aliasTryAtCall, defMap, resolveExprPath) ==
        nullptr);
}

TEST_CASE("ir lowerer call helpers keep lowered collection helper paths reachable via published surface ids") {
  primec::Definition loweredMapContainsDef;
  loweredMapContainsDef.fullPath = "/std/collections/map/contains";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/std/collections/map/contains", &loweredMapContainsDef},
  };
  const std::unordered_map<std::string, std::string> importAliases = {};

  primec::SemanticProgram semanticProgram;
  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      .scopePath = "/main",
      .callName = "contains",
      .sourceLine = 9,
      .sourceColumn = 4,
      .semanticNodeId = 81,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(
          semanticProgram, "/std/collections/map/contains"),
      .stdlibSurfaceId = primec::findStdlibSurfaceMetadataByBridgeKey("collections.map_helpers")->id,
  });
  semanticProgram.publishedRoutingLookups.directCallTargetIdsByExpr.insert_or_assign(
      81, semanticProgram.directCallTargets.front().resolvedPathId);
  semanticProgram.publishedRoutingLookups.directCallStdlibSurfaceIdsByExpr.insert_or_assign(
      81, primec::findStdlibSurfaceMetadataByBridgeKey("collections.map_helpers")->id);

  const auto resolveExprPath =
      primec::ir_lowerer::makeResolveCallPathFromScope(
          defMap,
          importAliases,
          &semanticProgram);

  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "/std/collections/map/contains";
  callExpr.semanticNodeId = 81;

  CHECK(resolveExprPath(callExpr) == "/std/collections/map/contains");
  CHECK(primec::ir_lowerer::resolveDefinitionCall(callExpr, defMap, resolveExprPath) ==
        &loweredMapContainsDef);
}

TEST_CASE("ir lowerer call helpers classify lowered map helper overloads through semantic surface ids") {
  primec::Definition loweredMapContainsOverloadDef;
  loweredMapContainsOverloadDef.fullPath = "/std/collections/map/contains__ov1";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {loweredMapContainsOverloadDef.fullPath, &loweredMapContainsOverloadDef},
  };

  primec::SemanticProgram semanticProgram;
  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      .scopePath = "/main",
      .callName = "contains",
      .sourceLine = 10,
      .sourceColumn = 6,
      .semanticNodeId = 83,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(
          semanticProgram, loweredMapContainsOverloadDef.fullPath),
      .stdlibSurfaceId = primec::findStdlibSurfaceMetadataByBridgeKey("collections.map_helpers")->id,
  });
  semanticProgram.publishedRoutingLookups.directCallTargetIdsByExpr.insert_or_assign(
      83, semanticProgram.directCallTargets.front().resolvedPathId);
  semanticProgram.publishedRoutingLookups.directCallStdlibSurfaceIdsByExpr.insert_or_assign(
      83, primec::findStdlibSurfaceMetadataByBridgeKey("collections.map_helpers")->id);

  const auto resolveExprPath = [&](const primec::Expr &) {
    return loweredMapContainsOverloadDef.fullPath;
  };

  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "contains";
  callExpr.semanticNodeId = 83;
  primec::Expr mapArg;
  mapArg.kind = primec::Expr::Kind::Name;
  mapArg.name = "values";
  primec::Expr keyArg;
  keyArg.kind = primec::Expr::Kind::Name;
  keyArg.name = "key";
  callExpr.args.push_back(mapArg);
  callExpr.args.push_back(keyArg);

  CHECK(primec::ir_lowerer::resolveDefinitionCall(
            callExpr, defMap, resolveExprPath, &semanticProgram) ==
        &loweredMapContainsOverloadDef);
}

TEST_CASE("ir lowerer call helpers leave experimental map helper calls ordinary") {
  primec::Definition experimentalMapCountDef;
  experimentalMapCountDef.fullPath = "/std/collections/experimental_map/mapCount";
  primec::Definition specializedMapMethodDef;
  specializedMapMethodDef.fullPath = "/std/collections/experimental_map/Map__t1234/count";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {experimentalMapCountDef.fullPath, &experimentalMapCountDef},
      {specializedMapMethodDef.fullPath, &specializedMapMethodDef},
  };

  const auto resolveExprPath = [&](const primec::Expr &) {
    return specializedMapMethodDef.fullPath;
  };

  primec::Expr countCall;
  countCall.kind = primec::Expr::Kind::Call;
  countCall.name = "/std/collections/experimental_map/mapCount";
  primec::Expr valuesArg;
  valuesArg.kind = primec::Expr::Kind::Name;
  valuesArg.name = "values";
  countCall.args.push_back(valuesArg);

  CHECK(primec::ir_lowerer::resolveDefinitionCall(countCall, defMap, resolveExprPath) ==
        &specializedMapMethodDef);

  primec::Expr bareCountCall = countCall;
  bareCountCall.name = "mapCount";

  CHECK(primec::ir_lowerer::resolveDefinitionCall(bareCountCall, defMap, resolveExprPath) ==
        &specializedMapMethodDef);

  primec::Definition specializedHelperDef;
  specializedHelperDef.fullPath = "/std/collections/experimental_map/mapCount__t1234";
  const std::unordered_map<std::string, const primec::Definition *> specializedDefMap = {
      {specializedHelperDef.fullPath, &specializedHelperDef},
  };
  const auto specializedResolveExprPath = [&](const primec::Expr &) {
    return specializedHelperDef.fullPath;
  };

  primec::Expr explicitSpecializedHelperCall;
  explicitSpecializedHelperCall.kind = primec::Expr::Kind::Call;
  explicitSpecializedHelperCall.name = specializedHelperDef.fullPath;
  explicitSpecializedHelperCall.args.push_back(valuesArg);

  CHECK(primec::ir_lowerer::resolveDefinitionCall(
            explicitSpecializedHelperCall, specializedDefMap, specializedResolveExprPath) ==
        &specializedHelperDef);

  primec::Expr explicitUnspecializedHelperCall;
  explicitUnspecializedHelperCall.kind = primec::Expr::Kind::Call;
  explicitUnspecializedHelperCall.name = "/std/collections/experimental_map/mapCount";
  explicitUnspecializedHelperCall.args.push_back(valuesArg);

  CHECK(primec::ir_lowerer::resolveDefinitionCall(
            explicitUnspecializedHelperCall, specializedDefMap, specializedResolveExprPath) ==
        &specializedHelperDef);

  primec::Definition competingRawHelperDef;
  competingRawHelperDef.fullPath = "/std/collections/experimental_map/mapCount";
  const std::unordered_map<std::string, const primec::Definition *> competingDefMap = {
      {competingRawHelperDef.fullPath, &competingRawHelperDef},
      {specializedHelperDef.fullPath, &specializedHelperDef},
      {specializedMapMethodDef.fullPath, &specializedMapMethodDef},
  };

  CHECK(primec::ir_lowerer::resolveDefinitionCall(
            explicitUnspecializedHelperCall, competingDefMap, resolveExprPath) ==
        &specializedMapMethodDef);
}

TEST_CASE("ir lowerer call helpers prefer specialized rooted raw defs when semantic rooted rewrites miss") {
  primec::Definition specializedHelperDef;
  specializedHelperDef.fullPath =
      "/std/collections/buffer_checked/bufferAlloc__t1234";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {specializedHelperDef.fullPath, &specializedHelperDef},
  };

  const auto resolveExprPath = [&](const primec::Expr &) {
    return std::string("/std/collections/buffer_checked/bufferAlloc");
  };

  primec::Expr allocCall;
  allocCall.kind = primec::Expr::Kind::Call;
  allocCall.name = specializedHelperDef.fullPath;
  allocCall.semanticNodeId = 41;
  primec::Expr countArg;
  countArg.kind = primec::Expr::Kind::Literal;
  countArg.literalValue = 1;
  allocCall.args.push_back(countArg);

  CHECK(primec::ir_lowerer::resolveDefinitionCall(allocCall, defMap, resolveExprPath) ==
        &specializedHelperDef);
}

TEST_CASE("ir lowerer bridge coverage uses published collection surface ids for lowered helper spellings") {
  primec::Program program;
  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "/std/collections/map/contains";
  callExpr.semanticNodeId = 82;
  mainDef.statements.push_back(callExpr);
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      .scopePath = "/main",
      .callName = "contains",
      .sourceLine = 11,
      .sourceColumn = 7,
      .semanticNodeId = 82,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(
          semanticProgram, "/std/collections/map/contains"),
      .stdlibSurfaceId = primec::findStdlibSurfaceMetadataByBridgeKey("collections.map_helpers")->id,
  });
  semanticProgram.publishedRoutingLookups.directCallTargetIdsByExpr.insert_or_assign(
      82, semanticProgram.directCallTargets.front().resolvedPathId);
  semanticProgram.publishedRoutingLookups.directCallStdlibSurfaceIdsByExpr.insert_or_assign(
      82, primec::findStdlibSurfaceMetadataByBridgeKey("collections.map_helpers")->id);

  std::string error;
  CHECK_FALSE(primec::ir_lowerer::validateSemanticProductBridgePathCoverage(
      program, &semanticProgram, error));
  CHECK(error.find("missing semantic-product bridge-path choice: /main -> /std/collections/map/contains") !=
        std::string::npos);
}

TEST_CASE("ir lowerer direct-call coverage uses published definition and import-alias lookups") {
  primec::Program program;
  program.imports.push_back("/pkg/*");

  primec::Definition helperDef;
  helperDef.fullPath = "/pkg/foo";
  helperDef.name = "foo";

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "foo";
  callExpr.semanticNodeId = 17;
  mainDef.statements.push_back(callExpr);

  program.definitions.push_back(helperDef);
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.imports = program.imports;
  semanticProgram.definitions.push_back(primec::SemanticProgramDefinition{
      .name = "foo",
      .fullPath = "/pkg/foo",
      .namespacePrefix = "/pkg",
  });
  semanticProgram.definitions.push_back(primec::SemanticProgramDefinition{
      .name = "main",
      .fullPath = "/main",
      .namespacePrefix = "",
  });

  const auto helperPathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/pkg/foo");
  const auto mainPathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/main");
  const auto aliasNameId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "foo");
  semanticProgram.publishedRoutingLookups.definitionIndicesByPathId.insert_or_assign(
      helperPathId, 0);
  semanticProgram.publishedRoutingLookups.definitionIndicesByPathId.insert_or_assign(
      mainPathId, 1);
  semanticProgram.publishedRoutingLookups.importAliasTargetPathIdsByNameId.insert_or_assign(
      aliasNameId, helperPathId);

  std::string error;
  CHECK_FALSE(primec::ir_lowerer::validateSemanticProductDirectCallCoverage(
      program, &semanticProgram, error));
  CHECK(error.find("missing semantic-product direct-call target: /main -> foo") !=
        std::string::npos);
}

TEST_CASE("ir lowerer call helpers keep semantic direct-call targets authoritative over rooted rewritten helper paths") {
  primec::Definition quatMultiplyDef;
  quatMultiplyDef.fullPath = "/std/math/quat_multiply_internal";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/std/math/quat_multiply_internal", &quatMultiplyDef},
  };
  const std::unordered_map<std::string, std::string> importAliases = {};

  primec::SemanticProgram semanticProgram;
  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      .scopePath = "/main",
      .callName = "multiply",
      .sourceLine = 12,
      .sourceColumn = 3,
      .semanticNodeId = 42,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/multiply"),
      .stdlibSurfaceId = std::nullopt,
  });
  semanticProgram.publishedRoutingLookups.directCallTargetIdsByExpr.insert_or_assign(
      42, primec::semanticProgramInternCallTargetString(semanticProgram, "/multiply"));

  const auto resolveExprPath =
      primec::ir_lowerer::makeResolveCallPathFromScope(
          defMap,
          importAliases,
          &semanticProgram);

  primec::Expr rewrittenExpr;
  rewrittenExpr.kind = primec::Expr::Kind::Call;
  rewrittenExpr.name = "/std/math/quat_multiply_internal";
  rewrittenExpr.semanticNodeId = 42;

  CHECK(resolveExprPath(rewrittenExpr) == "/multiply");
  CHECK(primec::ir_lowerer::resolveDefinitionCall(rewrittenExpr, defMap, resolveExprPath) == nullptr);
}

TEST_CASE("ir lowerer call helpers resolve definition namespace prefixes") {
  primec::Definition namespacedDef;
  namespacedDef.fullPath = "/pkg/foo";
  namespacedDef.namespacePrefix = "/pkg";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/pkg/foo", &namespacedDef},
      {"/pkg/null", nullptr},
  };

  CHECK(primec::ir_lowerer::resolveDefinitionNamespacePrefix(defMap, "/pkg/foo") == "/pkg");
  CHECK(primec::ir_lowerer::resolveDefinitionNamespacePrefix(defMap, "/pkg/missing").empty());
  CHECK(primec::ir_lowerer::resolveDefinitionNamespacePrefix(defMap, "/pkg/null").empty());
}

TEST_CASE("ir lowerer call helpers classify tail call candidates") {
  primec::Definition callee;
  callee.fullPath = "/callee";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {{"/callee", &callee}};
  const std::unordered_map<std::string, std::string> importAliases = {};
  auto resolveExprPath = [&](const primec::Expr &expr) {
    return primec::ir_lowerer::resolveCallPathFromScope(expr, defMap, importAliases);
  };

  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "callee";
  CHECK(primec::ir_lowerer::isTailCallCandidate(callExpr, defMap, resolveExprPath));

  primec::Expr methodCall = callExpr;
  methodCall.isMethodCall = true;
  CHECK_FALSE(primec::ir_lowerer::isTailCallCandidate(methodCall, defMap, resolveExprPath));

  primec::Expr nameExpr;
  nameExpr.kind = primec::Expr::Kind::Name;
  nameExpr.name = "callee";
  CHECK_FALSE(primec::ir_lowerer::isTailCallCandidate(nameExpr, defMap, resolveExprPath));

  primec::Expr unknownCall = callExpr;
  unknownCall.name = "missing";
  CHECK_FALSE(primec::ir_lowerer::isTailCallCandidate(unknownCall, defMap, resolveExprPath));
}

TEST_CASE("ir lowerer call helpers reject missing tail-call resolver") {
  primec::Definition callee;
  callee.fullPath = "/callee";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {{"/callee", &callee}};

  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "callee";

  CHECK_FALSE(primec::ir_lowerer::isTailCallCandidate(
      callExpr, defMap, primec::ExprStringFn{}));
}

TEST_CASE("ir lowerer call helpers build tail-call and definition-exists adapters") {
  primec::Definition callee;
  callee.fullPath = "/callee";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/callee", &callee},
      {"/null", nullptr},
  };
  const std::unordered_map<std::string, std::string> importAliases = {};

  auto resolveExprPath = primec::ir_lowerer::makeResolveCallPathFromScope(defMap, importAliases);
  auto isTailCallCandidate = primec::ir_lowerer::makeIsTailCallCandidate(defMap, resolveExprPath);
  auto definitionExists = primec::ir_lowerer::makeDefinitionExistsByPath(defMap);

  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "callee";
  CHECK(isTailCallCandidate(callExpr));

  primec::Expr unknownCall = callExpr;
  unknownCall.name = "missing";
  CHECK_FALSE(isTailCallCandidate(unknownCall));

  CHECK(definitionExists("/callee"));
  CHECK_FALSE(definitionExists("/missing"));
  CHECK_FALSE(definitionExists("/null"));
}

TEST_CASE("ir lowerer call helpers build bundled call-resolution adapters") {
  primec::Definition callee;
  callee.fullPath = "/callee";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/callee", &callee},
      {"/null", nullptr},
  };
  const std::unordered_map<std::string, std::string> importAliases = {{"callee", "/callee"}};

  auto adapters = primec::ir_lowerer::makeCallResolutionAdapters(defMap, importAliases);

  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "callee";
  CHECK(adapters.resolveExprPath(callExpr) == "/callee");
  CHECK(adapters.isTailCallCandidate(callExpr));

  primec::Expr unknownCall = callExpr;
  unknownCall.name = "missing";
  CHECK_FALSE(adapters.isTailCallCandidate(unknownCall));

  CHECK(adapters.definitionExists("/callee"));
  CHECK_FALSE(adapters.definitionExists("/missing"));
  CHECK_FALSE(adapters.definitionExists("/null"));
}

TEST_CASE("ir lowerer call helpers detect tail execution candidates from statements") {
  auto isTailCandidate = [](const primec::Expr &expr) {
    return expr.kind == primec::Expr::Kind::Call && expr.name == "callee";
  };

  std::vector<primec::Expr> statements;
  CHECK_FALSE(primec::ir_lowerer::hasTailExecutionCandidate(statements, true, isTailCandidate));

  primec::Expr directTail;
  directTail.kind = primec::Expr::Kind::Call;
  directTail.name = "callee";
  statements = {directTail};
  CHECK(primec::ir_lowerer::hasTailExecutionCandidate(statements, true, isTailCandidate));
  CHECK_FALSE(primec::ir_lowerer::hasTailExecutionCandidate(statements, false, isTailCandidate));
  CHECK_FALSE(primec::ir_lowerer::hasTailExecutionCandidate(
      statements, true, primec::ExprPredicateFn{}));

  primec::Expr returnCall;
  returnCall.kind = primec::Expr::Kind::Call;
  returnCall.name = "return";
  returnCall.args = {directTail};
  statements = {returnCall};
  CHECK(primec::ir_lowerer::hasTailExecutionCandidate(statements, false, isTailCandidate));

  primec::Expr nonTail;
  nonTail.kind = primec::Expr::Kind::Name;
  nonTail.name = "value";
  statements = {nonTail};
  CHECK_FALSE(primec::ir_lowerer::hasTailExecutionCandidate(statements, true, isTailCandidate));
}

TEST_SUITE_END();
