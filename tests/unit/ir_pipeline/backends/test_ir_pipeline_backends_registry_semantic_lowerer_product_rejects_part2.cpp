#include "third_party/doctest.h"

#include "test_ir_pipeline_backends_registry_shared.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.backends.registry");

TEST_CASE("bridge-path coverage rejects invalid helper name ids before lookup gaps") {
  primec::Program program;

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 5205;
  primec::Expr valuesExpr;
  valuesExpr.kind = primec::Expr::Kind::Name;
  valuesExpr.name = "values";
  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "count";
  callExpr.semanticNodeId = 5203;
  callExpr.args.push_back(valuesExpr);
  mainDef.statements.push_back(callExpr);
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  addVoidCallableSummary(semanticProgram, 5205);
  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      .scopePath = "/main",
      .callName = "count",
      .sourceLine = 1,
      .sourceColumn = 1,
      .semanticNodeId = 5203,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/vector/count"),
      .stdlibSurfaceId = std::nullopt,
  });
  semanticProgram.bridgePathChoices.push_back(primec::SemanticProgramBridgePathChoice{
      .scopePath = "/main",
      .collectionFamily = "vector",
      .sourceLine = 1,
      .sourceColumn = 1,
      .semanticNodeId = 5203,
      .helperNameId =
          static_cast<primec::SymbolId>(semanticProgram.callTargetStringTable.size() + 1u),
      .chosenPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/vector/count"),
      .stdlibSurfaceId = primec::StdlibSurfaceId::CollectionsManifestSurface0,
  });
  semanticProgram.publishedRoutingLookups.bridgePathChoiceIdsByExpr.insert_or_assign(
      5203, semanticProgram.bridgePathChoices.back().chosenPathId);

  std::string error;

  CHECK_FALSE(primec::ir_lowerer::validateSemanticProductBridgePathCoverage(
      program, &semanticProgram, error));
  CHECK(error == "missing semantic-product bridge helper name id: /main -> count");
}

TEST_CASE("ir lowerer rejects stale semantic-product bridge-path metadata") {
  primec::Program program;

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 5210;
  primec::Expr valuesExpr;
  valuesExpr.kind = primec::Expr::Kind::Name;
  valuesExpr.name = "values";
  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "count";
  callExpr.semanticNodeId = 5211;
  callExpr.args.push_back(valuesExpr);
  mainDef.statements.push_back(callExpr);
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  addVoidCallableSummary(semanticProgram, 5210);
  semanticProgram.bridgePathChoices.push_back(primec::SemanticProgramBridgePathChoice{
      .scopePath = "/main",
      .collectionFamily = "vector",
      .sourceLine = 1,
      .sourceColumn = 1,
      .semanticNodeId = 5211,
      .provenanceHandle = 0,
      .scopePathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
      .collectionFamilyId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "vector"),
      .helperNameId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "count"),
      .chosenPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/vector/count"),
      .stdlibSurfaceId = primec::StdlibSurfaceId::CollectionsManifestSurface0,
  });

  auto resetBridgeMetadata = [&]() {
    auto &bridgeChoice = semanticProgram.bridgePathChoices.back();
    bridgeChoice.scopePathId =
        primec::semanticProgramInternCallTargetString(semanticProgram, "/main");
    bridgeChoice.collectionFamilyId =
        primec::semanticProgramInternCallTargetString(semanticProgram, "vector");
    bridgeChoice.helperNameId =
        primec::semanticProgramInternCallTargetString(semanticProgram, "count");
    bridgeChoice.chosenPathId =
        primec::semanticProgramInternCallTargetString(semanticProgram, "/vector/count");
    semanticProgram.publishedRoutingLookups.bridgePathChoiceIdsByExpr.insert_or_assign(
        5211, bridgeChoice.chosenPathId);
  };

  std::string error;

  resetBridgeMetadata();
  semanticProgram.bridgePathChoices.back().chosenPathId = primec::InvalidSymbolId;
  CHECK_FALSE(primec::ir_lowerer::validateSemanticProductBridgePathCoverage(
      program, &semanticProgram, error));
  CHECK(error == "missing semantic-product bridge chosen path id: /main -> count");

  resetBridgeMetadata();
  semanticProgram.bridgePathChoices.back().scopePathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/other");
  error.clear();
  CHECK_FALSE(primec::ir_lowerer::validateSemanticProductBridgePathCoverage(
      program, &semanticProgram, error));
  CHECK(error == "stale semantic-product bridge scope metadata: /main -> count");

  resetBridgeMetadata();
  semanticProgram.bridgePathChoices.back().collectionFamilyId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "map");
  error.clear();
  CHECK_FALSE(primec::ir_lowerer::validateSemanticProductBridgePathCoverage(
      program, &semanticProgram, error));
  CHECK(error ==
        "stale semantic-product bridge collection family metadata: /main -> count");

  resetBridgeMetadata();
  semanticProgram.publishedRoutingLookups.bridgePathChoiceIdsByExpr.insert_or_assign(
      5211, primec::semanticProgramInternCallTargetString(semanticProgram, "/stale"));
  error.clear();
  CHECK_FALSE(primec::ir_lowerer::validateSemanticProductBridgePathCoverage(
      program, &semanticProgram, error));
  CHECK(error == "stale semantic-product bridge target metadata: /main -> count");
}

TEST_CASE("ir lowerer rejects missing semantic-product binding facts") {
  primec::Program program;

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  primec::Expr param;
  param.name = "value";
  param.semanticNodeId = 7;
  mainDef.parameters.push_back(param);
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "missing semantic-product binding fact: /main -> parameter value");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("ir lowerer rejects semantic-product binding facts missing resolved path ids") {
  primec::Program program;

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  primec::Expr param;
  param.name = "value";
  param.semanticNodeId = 7;
  mainDef.parameters.push_back(param);
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  semanticProgram.bindingFacts.push_back(primec::SemanticProgramBindingFact{
      .scopePath = "/main",
      .siteKind = "parameter",
      .name = "value",
      .bindingTypeText = "i32",
      .isMutable = false,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 7,
      .provenanceHandle = 0,
      .resolvedPathId = primec::InvalidSymbolId,
  });
  semanticProgram.publishedRoutingLookups.bindingFactIndicesByExpr.insert_or_assign(7, 0);

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "missing semantic-product binding resolved path id: /main -> parameter value");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("ir lowerer rejects missing semantic-product collection specializations") {
  primec::Program program;

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 81;
  primec::Transform returnTransform;
  returnTransform.name = "return";
  returnTransform.templateArgs.push_back("void");
  mainDef.transforms.push_back(returnTransform);

  primec::Expr bindingExpr;
  bindingExpr.isBinding = true;
  bindingExpr.name = "values";
  bindingExpr.semanticNodeId = 43;
  primec::Transform vectorTransform;
  vectorTransform.name = "vector";
  vectorTransform.templateArgs = {"i32"};
  bindingExpr.transforms.push_back(vectorTransform);
  mainDef.statements.push_back(bindingExpr);
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  addVoidCallableSummary(semanticProgram, 81);
  semanticProgram.bindingFacts.push_back(primec::SemanticProgramBindingFact{
      .scopePath = "/main",
      .siteKind = "local",
      .name = "values",
      .bindingTypeText = "vector<i32>",
      .isMutable = false,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 43,
      .provenanceHandle = 0,
      .resolvedPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/main/values"),
  });
  semanticProgram.publishedRoutingLookups.bindingFactIndicesByExpr.insert_or_assign(43, 0);

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "missing semantic-product collection specialization: /main -> local values");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("ir lowerer rejects missing semantic-product array extent facts") {
  primec::Program program;

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 81;
  primec::Transform returnTransform;
  returnTransform.name = "return";
  returnTransform.templateArgs.push_back("void");
  mainDef.transforms.push_back(returnTransform);

  primec::Expr bindingExpr;
  bindingExpr.isBinding = true;
  bindingExpr.name = "values";
  bindingExpr.semanticNodeId = 43;
  primec::Transform arrayTransform;
  arrayTransform.name = "array";
  arrayTransform.templateArgs = {"i32"};
  bindingExpr.transforms.push_back(arrayTransform);
  mainDef.statements.push_back(bindingExpr);
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  addVoidCallableSummary(semanticProgram, 81);
  semanticProgram.bindingFacts.push_back(primec::SemanticProgramBindingFact{
      .scopePath = "/main",
      .siteKind = "local",
      .name = "values",
      .bindingTypeText = "array<i32>",
      .isMutable = false,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 43,
      .provenanceHandle = 0,
      .resolvedPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/main/values"),
  });
  semanticProgram.publishedRoutingLookups.bindingFactIndicesByExpr.insert_or_assign(43, 0);

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "missing semantic-product array extent fact: /main -> local values");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("ir lowerer rejects stale semantic-product collection metadata") {
  primec::Program program;

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 81;
  primec::Transform mainReturnTransform;
  mainReturnTransform.name = "return";
  mainReturnTransform.templateArgs.push_back("void");
  mainDef.transforms.push_back(mainReturnTransform);

  primec::Expr bindingExpr;
  bindingExpr.isBinding = true;
  bindingExpr.name = "pairs";
  bindingExpr.semanticNodeId = 43;
  primec::Transform mapTransform;
  mapTransform.name = "map";
  mapTransform.templateArgs = {"i32", "i64"};
  bindingExpr.transforms.push_back(mapTransform);
  mainDef.statements.push_back(bindingExpr);
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  addVoidCallableSummary(semanticProgram, 81);
  semanticProgram.bindingFacts.push_back(primec::SemanticProgramBindingFact{
      .scopePath = "/main",
      .siteKind = "local",
      .name = "pairs",
      .bindingTypeText = "map<i32, i64>",
      .isMutable = false,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 43,
      .provenanceHandle = 0,
      .resolvedPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/main/pairs"),
  });
  semanticProgram.publishedRoutingLookups.bindingFactIndicesByExpr.insert_or_assign(43, 0);
  semanticProgram.collectionSpecializations.push_back(
      primec::SemanticProgramCollectionSpecialization{
          .scopePath = "/main",
          .siteKind = "local",
          .name = "pairs",
          .collectionFamily = "map",
          .bindingTypeText = "map<i32, i64>",
          .elementTypeText = "",
          .keyTypeText = "i32",
          .valueTypeText = "i64",
          .isReference = false,
          .isPointer = false,
          .sourceLine = 0,
          .sourceColumn = 0,
          .semanticNodeId = 43,
          .provenanceHandle = 0,
          .scopePathId =
              primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
          .siteKindId =
              primec::semanticProgramInternCallTargetString(semanticProgram, "local"),
          .nameId = primec::semanticProgramInternCallTargetString(semanticProgram, "pairs"),
          .collectionFamilyId =
              primec::semanticProgramInternCallTargetString(semanticProgram, "map"),
          .bindingTypeTextId =
              primec::semanticProgramInternCallTargetString(semanticProgram, "map<i32, i64>"),
          .elementTypeTextId = primec::InvalidSymbolId,
          .keyTypeTextId = primec::semanticProgramInternCallTargetString(semanticProgram, "i32"),
          .valueTypeTextId = primec::semanticProgramInternCallTargetString(semanticProgram, "i64"),
          .helperSurfaceId = std::nullopt,
          .constructorSurfaceId = std::nullopt,
      });
  semanticProgram.publishedRoutingLookups.collectionSpecializationIndicesByExpr.insert_or_assign(
      43, 0);

  auto refreshCollectionIds = [&]() {
    auto &collectionFact = semanticProgram.collectionSpecializations.back();
    collectionFact.collectionFamilyId =
        primec::semanticProgramInternCallTargetString(semanticProgram, "map");
    collectionFact.bindingTypeTextId =
        primec::semanticProgramInternCallTargetString(semanticProgram, "map<i32, i64>");
    collectionFact.elementTypeTextId = primec::InvalidSymbolId;
    collectionFact.keyTypeTextId =
        primec::semanticProgramInternCallTargetString(semanticProgram, "i32");
    collectionFact.valueTypeTextId =
        primec::semanticProgramInternCallTargetString(semanticProgram, "i64");
  };

  auto lowerWithSemanticProduct = [&](std::string &error,
                                      primec::DiagnosticSinkReport &diagnosticInfo) {
    const bool ok = primec::ir_lowerer::validateSemanticProductCollectionSpecializationCoverage(
        program, &semanticProgram, error);
    diagnosticInfo.message = error;
    return ok;
  };

  std::string error;
  primec::DiagnosticSinkReport diagnosticInfo;

  semanticProgram.bindingFacts.back().bindingTypeTextId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "map<i32, i64>");
  semanticProgram.bindingFacts.back().bindingTypeText = "";
  refreshCollectionIds();
  CAPTURE(error);
  CAPTURE(diagnosticInfo.message);
  CHECK(lowerWithSemanticProduct(error, diagnosticInfo));
  CHECK(error.empty());
  CHECK(diagnosticInfo.message.empty());

  refreshCollectionIds();
  error.clear();
  diagnosticInfo = {};
  semanticProgram.collectionSpecializations.back().collectionFamilyId =
      static_cast<primec::SymbolId>(semanticProgram.callTargetStringTable.size() + 1u);
  CHECK_FALSE(lowerWithSemanticProduct(error, diagnosticInfo));
  CHECK(error == "missing semantic-product collection specialization family id: /main -> local pairs");
  CHECK(diagnosticInfo.message == error);

  refreshCollectionIds();
  semanticProgram.collectionSpecializations.back().bindingTypeTextId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "map<i32, i32>");
  error.clear();
  diagnosticInfo = {};
  CHECK_FALSE(lowerWithSemanticProduct(error, diagnosticInfo));
  CHECK(error ==
        "stale semantic-product collection specialization binding type metadata: /main -> local pairs");
  CHECK(diagnosticInfo.message == error);

  refreshCollectionIds();
  semanticProgram.collectionSpecializations.back().elementTypeTextId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "i32");
  error.clear();
  diagnosticInfo = {};
  CHECK_FALSE(lowerWithSemanticProduct(error, diagnosticInfo));
  CHECK(error ==
        "stale semantic-product collection specialization element type metadata: /main -> local pairs");
  CHECK(diagnosticInfo.message == error);

  refreshCollectionIds();
  semanticProgram.collectionSpecializations.back().keyTypeTextId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "i64");
  error.clear();
  diagnosticInfo = {};
  CHECK_FALSE(lowerWithSemanticProduct(error, diagnosticInfo));
  CHECK(error ==
        "stale semantic-product collection specialization key type metadata: /main -> local pairs");
  CHECK(diagnosticInfo.message == error);

  refreshCollectionIds();
  semanticProgram.collectionSpecializations.back().valueTypeTextId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "i32");
  error.clear();
  diagnosticInfo = {};
  CHECK_FALSE(lowerWithSemanticProduct(error, diagnosticInfo));
  CHECK(error ==
        "stale semantic-product collection specialization value type metadata: /main -> local pairs");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("ir lowerer rejects missing semantic-product local binding facts") {
  primec::Program program;

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 81;

  primec::Expr initializerExpr;
  initializerExpr.kind = primec::Expr::Kind::Literal;
  initializerExpr.literalValue = 1;
  initializerExpr.intWidth = 32;

  primec::Expr bindingExpr;
  bindingExpr.isBinding = true;
  bindingExpr.name = "selected";
  bindingExpr.semanticNodeId = 43;
  bindingExpr.args.push_back(initializerExpr);
  mainDef.statements.push_back(bindingExpr);
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
  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "missing semantic-product binding fact: /main -> local selected");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("ir lowerer rejects missing semantic-product type metadata for struct layouts") {
  primec::Program program;

  primec::Definition structDef;
  structDef.fullPath = "/Point";
  primec::Transform structTransform;
  structTransform.name = "struct";
  structDef.transforms.push_back(structTransform);
  program.definitions.push_back(structDef);

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 81;
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  addVoidCallableSummary(semanticProgram, 81);

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "missing semantic-product type metadata: /Point");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("ir lowerer rejects missing semantic-product struct field metadata") {
  primec::Program program;

  primec::Definition structDef;
  structDef.fullPath = "/Point";
  primec::Transform structTransform;
  structTransform.name = "struct";
  structDef.transforms.push_back(structTransform);
  primec::Expr field;
  field.kind = primec::Expr::Kind::Name;
  field.isBinding = true;
  field.name = "x";
  structDef.statements.push_back(field);
  program.definitions.push_back(structDef);

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 81;
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  addVoidCallableSummary(semanticProgram, 81);
  semanticProgram.typeMetadata.push_back(primec::SemanticProgramTypeMetadata{
      .fullPath = "/Point",
      .category = "struct",
      .isPublic = false,
      .hasNoPadding = false,
      .hasPlatformIndependentPadding = false,
      .hasExplicitAlignment = false,
      .explicitAlignmentBytes = 0,
      .fieldCount = 1,
      .enumValueCount = 0,
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 82,
      .provenanceHandle = 0,
  });

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "missing semantic-product struct field metadata: /Point/x");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("ir lowerer rejects stale semantic-product struct provenance") {
  primec::Program program;

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 81;
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  addVoidCallableSummary(semanticProgram, 81);
  semanticProgram.typeMetadata.push_back(primec::SemanticProgramTypeMetadata{
      .fullPath = "/Ghost",
      .category = "struct",
      .isPublic = false,
      .hasNoPadding = false,
      .hasPlatformIndependentPadding = false,
      .hasExplicitAlignment = false,
      .explicitAlignmentBytes = 0,
      .fieldCount = 0,
      .enumValueCount = 0,
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 82,
      .provenanceHandle = 0,
  });

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "missing semantic-product struct provenance: /Ghost");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("ir lowerer completeness checks keep deterministic first-failure order") {
  primec::Program program;

  primec::Definition callee;
  callee.fullPath = "/callee";
  callee.semanticNodeId = 82;
  program.definitions.push_back(callee);

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 81;

  primec::Expr param;
  param.name = "value";
  param.semanticNodeId = 45;
  primec::Transform paramArrayTransform;
  paramArrayTransform.name = "array";
  paramArrayTransform.templateArgs = {"string"};
  param.transforms.push_back(paramArrayTransform);
  mainDef.parameters.push_back(param);

  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "callee";
  callExpr.semanticNodeId = 46;
  mainDef.statements.push_back(callExpr);

  primec::Expr initializerExpr;
  initializerExpr.kind = primec::Expr::Kind::Literal;
  initializerExpr.literalValue = 1;
  initializerExpr.intWidth = 32;

  primec::Expr bindingExpr;
  bindingExpr.isBinding = true;
  bindingExpr.name = "selected";
  bindingExpr.semanticNodeId = 47;
  bindingExpr.args.push_back(initializerExpr);
  mainDef.statements.push_back(bindingExpr);
  program.definitions.push_back(mainDef);

  primec::IrLowerer lowerer;
  auto lowerWithSemanticProduct = [&](primec::SemanticProgram &semanticProgram,
                                      std::string &errorOut,
                                      primec::DiagnosticSinkReport &diagnosticOut) {
    primec::IrModule module;
    return lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, errorOut, &diagnosticOut);
  };

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
      .semanticNodeId = 82,
      .provenanceHandle = 0,
      .fullPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/callee"),
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
      .semanticNodeId = 82,
      .definitionPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/callee"),
  });
  semanticProgram.publishedRoutingLookups.returnFactIndicesByDefinitionId
      .insert_or_assign(82, semanticProgram.returnFacts.size() - 1);
  publishFixtureCallableAndReturnRouting(semanticProgram);

  std::string error;
  primec::DiagnosticSinkReport diagnosticInfo;
  CHECK_FALSE(lowerWithSemanticProduct(semanticProgram, error, diagnosticInfo));
  CHECK(error == "missing semantic-product binding fact: /main -> parameter value");
  CHECK(diagnosticInfo.message == error);

  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      .scopePath = "/main",
      .callName = "callee",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 46,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/callee"),
      .stdlibSurfaceId = std::nullopt,
  });

  error.clear();
  diagnosticInfo = {};
  CHECK_FALSE(lowerWithSemanticProduct(semanticProgram, error, diagnosticInfo));
  CHECK(error == "missing semantic-product binding fact: /main -> parameter value");
  CHECK(diagnosticInfo.message == error);

  semanticProgram.bindingFacts.push_back(primec::SemanticProgramBindingFact{
      .scopePath = "/main",
      .siteKind = "parameter",
      .name = "value",
      .bindingTypeText = "array<string>",
      .isMutable = false,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 45,
      .provenanceHandle = 0,
      .resolvedPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/main/value"),
  });
  semanticProgram.publishedRoutingLookups.bindingFactIndicesByExpr.insert_or_assign(45, 0);
  semanticProgram.bindingFacts.push_back(primec::SemanticProgramBindingFact{
      .scopePath = "/main",
      .siteKind = "local",
      .name = "selected",
      .bindingTypeText = "i32",
      .isMutable = false,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 47,
      .provenanceHandle = 0,
      .resolvedPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/main/selected"),
  });
  semanticProgram.publishedRoutingLookups.bindingFactIndicesByExpr.insert_or_assign(47, 1);

  semanticProgram.bindingFacts.back().bindingTypeTextId =
      static_cast<primec::SymbolId>(semanticProgram.callTargetStringTable.size() + 1u);
  error.clear();
  diagnosticInfo = {};
  CHECK_FALSE(lowerWithSemanticProduct(semanticProgram, error, diagnosticInfo));
  CHECK(error == "missing semantic-product binding type id: /main -> local selected");
  CHECK(diagnosticInfo.message == error);

  semanticProgram.bindingFacts.back().bindingTypeTextId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "i64");
  error.clear();
  diagnosticInfo = {};
  CHECK_FALSE(lowerWithSemanticProduct(semanticProgram, error, diagnosticInfo));
  CHECK(error == "stale semantic-product binding type metadata: /main -> local selected");
  CHECK(diagnosticInfo.message == error);

  semanticProgram.bindingFacts.back().bindingTypeTextId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "i32");
  semanticProgram.bindingFacts.back().referenceRoot = "selected";
  semanticProgram.bindingFacts.back().referenceRootId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "other");
  error.clear();
  diagnosticInfo = {};
  CHECK_FALSE(lowerWithSemanticProduct(semanticProgram, error, diagnosticInfo));
  CHECK(error ==
        "stale semantic-product binding reference root metadata: /main -> local selected");
  CHECK(diagnosticInfo.message == error);

  semanticProgram.bindingFacts.back().referenceRoot = "";
  semanticProgram.bindingFacts.back().referenceRootId = primec::InvalidSymbolId;
  semanticProgram.bindingFacts.back().resolvedPathId =
      static_cast<primec::SymbolId>(semanticProgram.callTargetStringTable.size() + 1u);
  error.clear();
  diagnosticInfo = {};
  CHECK_FALSE(lowerWithSemanticProduct(semanticProgram, error, diagnosticInfo));
  CHECK(error == "missing semantic-product binding resolved path id: /main -> local selected");
  CHECK(diagnosticInfo.message == error);

  semanticProgram.bindingFacts.back().resolvedPathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/main/selected");
  primec::SemanticProgramLocalAutoFact localAutoFact;
  localAutoFact.scopePath = "/main";
  localAutoFact.bindingName = "selected";
  localAutoFact.bindingTypeText = "i32";
  localAutoFact.semanticNodeId = 47;
  localAutoFact.bindingTypeTextId =
      static_cast<primec::SymbolId>(semanticProgram.callTargetStringTable.size() + 1u);
  localAutoFact.initializerResolvedPathId =
      static_cast<primec::SymbolId>(semanticProgram.callTargetStringTable.size() + 1u);
  semanticProgram.localAutoFacts.push_back(std::move(localAutoFact));
  semanticProgram.publishedRoutingLookups.localAutoFactIndicesByExpr.insert_or_assign(47, 0);
  error.clear();
  diagnosticInfo = {};
  CHECK_FALSE(lowerWithSemanticProduct(semanticProgram, error, diagnosticInfo));
  CHECK(error == "missing semantic-product local-auto binding type id: /main -> local selected");
  CHECK(diagnosticInfo.message == error);

  semanticProgram.localAutoFacts.back().bindingTypeTextId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "i64");
  error.clear();
  diagnosticInfo = {};
  CHECK_FALSE(lowerWithSemanticProduct(semanticProgram, error, diagnosticInfo));
  CHECK(error ==
        "stale semantic-product local-auto binding type metadata: /main -> local selected");
  CHECK(diagnosticInfo.message == error);

  semanticProgram.localAutoFacts.back().bindingTypeTextId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "i32");
  semanticProgram.localAutoFacts.back().initializerResolvedPathId =
      static_cast<primec::SymbolId>(semanticProgram.callTargetStringTable.size() + 1u);
  error.clear();
  diagnosticInfo = {};
  CHECK_FALSE(lowerWithSemanticProduct(semanticProgram, error, diagnosticInfo));
  CHECK(error == "missing semantic-product local-auto initializer path id: /main -> local selected");
  CHECK(diagnosticInfo.message == error);

  semanticProgram.localAutoFacts.back().initializerResolvedPathId = primec::InvalidSymbolId;
  error.clear();
  diagnosticInfo = {};
  CHECK(lowerWithSemanticProduct(semanticProgram, error, diagnosticInfo));
  CHECK(error.empty());
  CHECK(diagnosticInfo.message.empty());

  semanticProgram.localAutoFacts.back().initializerResolvedPathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/callee");
  semanticProgram.localAutoFacts.back().initializerDirectCallResolvedPathId =
      static_cast<primec::SymbolId>(semanticProgram.callTargetStringTable.size() + 1u);
  error.clear();
  diagnosticInfo = {};
  CHECK_FALSE(lowerWithSemanticProduct(semanticProgram, error, diagnosticInfo));
  CHECK(error ==
        "missing semantic-product local-auto direct-call path id: /main -> local selected");
  CHECK(diagnosticInfo.message == error);

  semanticProgram.localAutoFacts.back().initializerDirectCallResolvedPathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/stale_callee");
  error.clear();
  diagnosticInfo = {};
  CHECK_FALSE(lowerWithSemanticProduct(semanticProgram, error, diagnosticInfo));
  CHECK(error ==
        "stale semantic-product local-auto direct-call fact: /main -> local selected");
  CHECK(diagnosticInfo.message == error);

  semanticProgram.localAutoFacts.back().initializerDirectCallResolvedPathId =
      primec::InvalidSymbolId;
  semanticProgram.localAutoFacts.back().initializerMethodCallResolvedPathId =
      static_cast<primec::SymbolId>(semanticProgram.callTargetStringTable.size() + 1u);
  error.clear();
  diagnosticInfo = {};
  CHECK_FALSE(lowerWithSemanticProduct(semanticProgram, error, diagnosticInfo));
  CHECK(error ==
        "missing semantic-product local-auto method-call path id: /main -> local selected");
  CHECK(diagnosticInfo.message == error);

  semanticProgram.localAutoFacts.back().initializerMethodCallResolvedPathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/stale_callee");
  error.clear();
  diagnosticInfo = {};
  CHECK_FALSE(lowerWithSemanticProduct(semanticProgram, error, diagnosticInfo));
  CHECK(error ==
        "stale semantic-product local-auto method-call fact: /main -> local selected");
  CHECK(diagnosticInfo.message == error);

  semanticProgram.localAutoFacts.back().initializerMethodCallResolvedPathId =
      primec::InvalidSymbolId;
  semanticProgram.localAutoFacts.back().initializerResolvedPathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/callee");
  semanticProgram.localAutoFacts.back().initializerDirectCallResolvedPathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/callee");
  semanticProgram.localAutoFacts.back().initializerDirectCallReturnKindId =
      static_cast<primec::SymbolId>(semanticProgram.callTargetStringTable.size() + 1u);
  error.clear();
  diagnosticInfo = {};
  CHECK_FALSE(lowerWithSemanticProduct(semanticProgram, error, diagnosticInfo));
  CHECK(error ==
        "missing semantic-product local-auto direct-call return-kind id: /main -> local selected");
  CHECK(diagnosticInfo.message == error);

  semanticProgram.localAutoFacts.back().initializerDirectCallReturnKind = "i64";
  semanticProgram.localAutoFacts.back().initializerDirectCallReturnKindId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "i64");
  error.clear();
  diagnosticInfo = {};
  CHECK_FALSE(lowerWithSemanticProduct(semanticProgram, error, diagnosticInfo));
  CHECK(error ==
        "stale semantic-product local-auto direct-call return-kind fact: /main -> local selected");
  CHECK(diagnosticInfo.message == error);

  semanticProgram.localAutoFacts.back().initializerDirectCallResolvedPathId =
      primec::InvalidSymbolId;
  semanticProgram.localAutoFacts.back().initializerDirectCallReturnKind = "void";
  semanticProgram.localAutoFacts.back().initializerDirectCallReturnKindId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "void");
  semanticProgram.localAutoFacts.back().initializerMethodCallResolvedPathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/callee");
  semanticProgram.localAutoFacts.back().initializerMethodCallReturnKind = "i64";
  semanticProgram.localAutoFacts.back().initializerMethodCallReturnKindId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "i64");
  error.clear();
  diagnosticInfo = {};
  CHECK_FALSE(lowerWithSemanticProduct(semanticProgram, error, diagnosticInfo));
  CHECK(error ==
        "stale semantic-product local-auto method-call return-kind fact: /main -> local selected");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("semantic-product local-auto call paths accept stdlib surface equivalents") {
  primec::Program program;
  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  primec::Expr selected;
  selected.isBinding = true;
  selected.name = "selected";
  selected.semanticNodeId = 47;
  primec::Expr initializer;
  initializer.kind = primec::Expr::Kind::Call;
  initializer.name = "/std/collections/vectorAt__t25a78a513414c3bf";
  selected.args.push_back(initializer);
  mainDef.statements.push_back(selected);
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  primec::SemanticProgramLocalAutoFact localAutoFact;
  localAutoFact.scopePath = "/main";
  localAutoFact.bindingName = "selected";
  localAutoFact.bindingTypeText = "i32";
  localAutoFact.initializerDirectCallResolvedPath =
      "/std/collections/vectorAt__t25a78a513414c3bf";
  localAutoFact.initializerDirectCallReturnKind = "i32";
  localAutoFact.initializerStdlibSurfaceId =
      primec::StdlibSurfaceId::CollectionsManifestSurface0;
  localAutoFact.initializerDirectCallStdlibSurfaceId =
      primec::StdlibSurfaceId::CollectionsManifestSurface0;
  localAutoFact.semanticNodeId = 47;
  localAutoFact.bindingTypeTextId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "i32");
  localAutoFact.bindingTypeText = "";
  localAutoFact.initializerResolvedPathId =
      primec::semanticProgramInternCallTargetString(semanticProgram,
                                                    "/std/collections/vector/at");
  localAutoFact.initializerDirectCallResolvedPathId =
      primec::semanticProgramInternCallTargetString(
          semanticProgram, "/std/collections/vectorAt__t25a78a513414c3bf");
  localAutoFact.initializerDirectCallReturnKindId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "i32");
  semanticProgram.localAutoFacts.push_back(localAutoFact);
  semanticProgram.publishedRoutingLookups.localAutoFactIndicesByExpr.insert_or_assign(47, 0);

  std::string error;
  CHECK(primec::ir_lowerer::validateSemanticProductLocalAutoCoverage(
      program, &semanticProgram, error));
  CHECK(error.empty());

  semanticProgram.localAutoFacts.back().initializerDirectCallStdlibSurfaceId =
      primec::findStdlibSurfaceMetadataByBridgeKey("collections.map_helpers")->id;
  semanticProgram.localAutoFacts.back().initializerDirectCallResolvedPathId =
      primec::semanticProgramInternCallTargetString(
          semanticProgram, "/std/collections/map/contains__t25a78a513414c3bf");
  CHECK_FALSE(primec::ir_lowerer::validateSemanticProductLocalAutoCoverage(
      program, &semanticProgram, error));
  CHECK(error ==
        "stale semantic-product local-auto direct-call fact: /main -> local selected");
}

TEST_SUITE_END();
