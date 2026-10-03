#include "third_party/doctest.h"

#include "test_ir_pipeline_backends_registry_shared.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.backends.registry");

TEST_CASE("semantic-product local-auto call paths accept specialized direct calls") {
  primec::Program program;
  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  primec::Expr selected;
  selected.isBinding = true;
  selected.name = "selected";
  selected.semanticNodeId = 47;
  primec::Expr initializer;
  initializer.kind = primec::Expr::Kind::Call;
  initializer.name = "wrapValues";
  selected.args.push_back(initializer);
  mainDef.statements.push_back(selected);
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  primec::SemanticProgramLocalAutoFact localAutoFact;
  localAutoFact.scopePath = "/main";
  localAutoFact.bindingName = "selected";
  localAutoFact.bindingTypeText = "map<string, i32>";
  localAutoFact.initializerDirectCallResolvedPath =
      "/wrapValues__t9b7bdbb33f7d43aa";
  localAutoFact.semanticNodeId = 47;
  localAutoFact.bindingTypeTextId =
      primec::semanticProgramInternCallTargetString(semanticProgram,
                                                    "map<string, i32>");
  localAutoFact.initializerResolvedPathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/wrapValues");
  localAutoFact.initializerDirectCallResolvedPathId =
      primec::semanticProgramInternCallTargetString(
          semanticProgram, "/wrapValues__t9b7bdbb33f7d43aa");
  semanticProgram.localAutoFacts.push_back(localAutoFact);
  semanticProgram.publishedRoutingLookups.localAutoFactIndicesByExpr.insert_or_assign(47, 0);

  std::string error;
  CHECK(primec::ir_lowerer::validateSemanticProductLocalAutoCoverage(
      program, &semanticProgram, error));
  CHECK(error.empty());

  semanticProgram.localAutoFacts.back().initializerDirectCallResolvedPath =
      "/otherWrap__t9b7bdbb33f7d43aa";
  semanticProgram.localAutoFacts.back().initializerDirectCallResolvedPathId =
      primec::semanticProgramInternCallTargetString(
          semanticProgram, "/otherWrap__t9b7bdbb33f7d43aa");
  CHECK_FALSE(primec::ir_lowerer::validateSemanticProductLocalAutoCoverage(
      program, &semanticProgram, error));
  CHECK(error ==
        "stale semantic-product local-auto direct-call fact: /main -> local selected");
}

TEST_CASE("semantic-product local-auto call paths accept overloaded specialized direct calls") {
  primec::Program program;
  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  primec::Expr selected;
  selected.isBinding = true;
  selected.name = "selected";
  selected.semanticNodeId = 47;
  primec::Expr initializer;
  initializer.kind = primec::Expr::Kind::Call;
  initializer.name = "wrapValues";
  selected.args.push_back(initializer);
  mainDef.statements.push_back(selected);
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  primec::SemanticProgramLocalAutoFact localAutoFact;
  localAutoFact.scopePath = "/main";
  localAutoFact.bindingName = "selected";
  localAutoFact.bindingTypeText = "vector<i32>";
  localAutoFact.initializerDirectCallResolvedPath =
      "/wrapValues__ov2__t9b7bdbb33f7d43aa";
  localAutoFact.semanticNodeId = 47;
  localAutoFact.bindingTypeTextId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "vector<i32>");
  localAutoFact.initializerResolvedPathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/wrapValues__ov2");
  localAutoFact.initializerDirectCallResolvedPathId =
      primec::semanticProgramInternCallTargetString(
          semanticProgram, "/wrapValues__ov2__t9b7bdbb33f7d43aa");
  semanticProgram.localAutoFacts.push_back(localAutoFact);
  semanticProgram.publishedRoutingLookups.localAutoFactIndicesByExpr.insert_or_assign(47, 0);

  std::string error;
  CHECK(primec::ir_lowerer::validateSemanticProductLocalAutoCoverage(
      program, &semanticProgram, error));
  CHECK(error.empty());
}

TEST_CASE("ir preparation rejects semantic-product local-auto path fallback in production mode") {
  primec::Program program;

  primec::Definition calleeDef;
  calleeDef.fullPath = "/callee";
  calleeDef.semanticNodeId = 183;
  primec::Expr calleeReturnExpr;
  calleeReturnExpr.kind = primec::Expr::Kind::Literal;
  calleeReturnExpr.literalValue = 1;
  calleeReturnExpr.intWidth = 32;
  calleeDef.returnExpr = calleeReturnExpr;
  program.definitions.push_back(calleeDef);

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 181;
  primec::Expr initCall;
  initCall.kind = primec::Expr::Kind::Call;
  initCall.name = "callee";
  initCall.semanticNodeId = 182;
  primec::Expr localBinding;
  localBinding.isBinding = true;
  localBinding.name = "selected";
  localBinding.semanticNodeId = 184;
  localBinding.args.push_back(initCall);
  mainDef.statements.push_back(localBinding);
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  semanticProgram.callableSummaries.push_back(primec::SemanticProgramCallableSummary{
      .isExecution = false,
      .returnKind = "return",
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
      .semanticNodeId = 183,
      .provenanceHandle = 0,
      .fullPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/callee"),
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
      .semanticNodeId = 181,
      .provenanceHandle = 0,
      .fullPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
  });
  semanticProgram.returnFacts.push_back(primec::SemanticProgramReturnFact{
      .returnKind = "return",
      .structPath = "",
      .bindingTypeText = "i32",
      .isMutable = false,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 183,
      .definitionPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/callee"),
  });
  semanticProgram.publishedRoutingLookups.returnFactIndicesByDefinitionId
      .insert_or_assign(183, semanticProgram.returnFacts.size() - 1);
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
      .semanticNodeId = 181,
      .definitionPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
  });
  semanticProgram.publishedRoutingLookups.returnFactIndicesByDefinitionId
      .insert_or_assign(181, semanticProgram.returnFacts.size() - 1);
  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      .scopePath = "/main",
      .callName = "callee",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 182,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/callee"),
      .stdlibSurfaceId = std::nullopt,
  });
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
      .semanticNodeId = 184,
      .provenanceHandle = 0,
      .resolvedPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/main/selected"),
  });
  semanticProgram.publishedRoutingLookups.bindingFactIndicesByExpr.insert_or_assign(184, 0);
  semanticProgram.localAutoFacts.push_back(primec::SemanticProgramLocalAutoFact{
      .scopePath = "/main",
      .bindingName = "selected",
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
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 0,
      .provenanceHandle = 0,
      .initializerDirectCallResolvedPath = "",
      .initializerDirectCallReturnKind = "",
      .initializerMethodCallResolvedPath = "",
      .initializerMethodCallReturnKind = "",
      .initializerStdlibSurfaceId = std::nullopt,
      .initializerDirectCallStdlibSurfaceId = std::nullopt,
      .initializerMethodCallStdlibSurfaceId = std::nullopt,
      .bindingNameId = primec::semanticProgramInternCallTargetString(semanticProgram, "selected"),
      .initializerResolvedPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/callee"),
  });

  primec::Options options;
  options.entryPath = "/main";

  primec::IrModule ir;
  primec::IrPreparationFailure failure;
  CHECK_FALSE(primec::prepareIrModule(
      program, &semanticProgram, options, primec::IrValidationTarget::Vm, ir, failure));
  CHECK(failure.stage == primec::IrPreparationFailureStage::Lowering);
  CHECK(failure.message == "missing semantic-product local-auto fact: /main -> local selected");
  CHECK(failure.diagnosticInfo.message == failure.message);
}

TEST_CASE("ir lowerer rejects missing semantic-product callable summaries") {
  primec::Program program;

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "missing semantic-product callable summary: /main");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("ir lowerer effect validation skips semantic-product sum definitions") {
  primec::Program program;

  primec::Definition sumDef;
  sumDef.fullPath = "/std/result/Result__arity2__tabc";
  sumDef.semanticNodeId = 81;
  program.definitions.push_back(sumDef);

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 82;
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  semanticProgram.typeMetadata.push_back(primec::SemanticProgramTypeMetadata{
      .fullPath = sumDef.fullPath,
      .category = "sum",
      .semanticNodeId = sumDef.semanticNodeId,
  });
  semanticProgram.sumTypeMetadata.push_back(primec::SemanticProgramSumTypeMetadata{
      .fullPath = sumDef.fullPath,
      .isPublic = false,
      .activeTagTypeText = "u32",
      .payloadStorageText = "array",
      .variantCount = 2,
      .semanticNodeId = sumDef.semanticNodeId,
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
      .semanticNodeId = mainDef.semanticNodeId,
      .provenanceHandle = 0,
      .fullPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
  });
  publishFixtureCallableAndReturnRouting(semanticProgram);

  std::string error;
  CHECK(primec::ir_lowerer::validateProgramEffectsForBackendSurface(
      program, &semanticProgram, "/main", {}, {}, "native backend", error));
  CHECK(error.empty());
}

TEST_CASE("ir lowerer rejects missing semantic-product entry parameter facts") {
  primec::Program program;

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 81;
  primec::Expr entryParam;
  entryParam.name = "argv";
  primec::Transform arrayTransform;
  arrayTransform.name = "array";
  arrayTransform.templateArgs = {"string"};
  entryParam.transforms.push_back(arrayTransform);
  mainDef.parameters.push_back(entryParam);
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
  CHECK(error == "missing semantic-product entry parameter fact: /main");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("ir lowerer rejects missing semantic-product return facts") {
  primec::Program program;

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 81;
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  semanticProgram.callableSummaries.push_back(primec::SemanticProgramCallableSummary{
      .isExecution = false,
      .returnKind = "return",
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
  publishFixtureCallableAndReturnRouting(semanticProgram);

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "missing semantic-product return fact: /main");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("ir lowerer rejects missing semantic-product callable result metadata") {
  primec::Program program;

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 82;
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  semanticProgram.callableSummaries.push_back(primec::SemanticProgramCallableSummary{
      .isExecution = false,
      .returnKind = "return",
      .isCompute = false,
      .isUnsafe = false,
      .activeEffects = {},
      .activeCapabilities = {},
      .hasResultType = true,
      .resultTypeHasValue = true,
      .resultValueType = "",
      .resultErrorType = "FileError",
      .hasOnError = false,
      .onErrorHandlerPath = "",
      .onErrorErrorType = "",
      .onErrorBoundArgCount = 0,
      .semanticNodeId = 82,
      .provenanceHandle = 0,
      .fullPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
  });
  semanticProgram.returnFacts.push_back(primec::SemanticProgramReturnFact{
      .returnKind = "return",
      .structPath = "",
      .bindingTypeText = "Result<i32, FileError>",
      .isMutable = false,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 82,
      .definitionPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
  });
  semanticProgram.publishedRoutingLookups.returnFactIndicesByDefinitionId
      .insert_or_assign(82, semanticProgram.returnFacts.size() - 1);

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "missing semantic-product callable result metadata: /main");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("ir lowerer callable result metadata uses interned value ids") {
  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  semanticProgram.callableSummaries.push_back(primec::SemanticProgramCallableSummary{
      .isExecution = false,
      .returnKind = "return",
      .isCompute = false,
      .isUnsafe = false,
      .activeEffects = {},
      .activeCapabilities = {},
      .hasResultType = true,
      .resultTypeHasValue = true,
      .resultValueType = "",
      .resultErrorType = "FileError",
      .hasOnError = false,
      .onErrorHandlerPath = "",
      .onErrorErrorType = "",
      .onErrorBoundArgCount = 0,
      .semanticNodeId = 8202,
      .provenanceHandle = 0,
      .fullPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
      .resultValueTypeId = primec::semanticProgramInternCallTargetString(semanticProgram, "i32"),
      .resultErrorTypeId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "FileError"),
  });

  std::string error;
  CHECK(primec::ir_lowerer::validateSemanticProductResultMetadataCompleteness(
      &semanticProgram, error));
  CHECK(error.empty());

  semanticProgram.callableSummaries.back().resultValueTypeId =
      static_cast<primec::SymbolId>(semanticProgram.callTargetStringTable.size() + 1u);
  CHECK_FALSE(primec::ir_lowerer::validateSemanticProductResultMetadataCompleteness(
      &semanticProgram, error));
  CHECK(error == "missing semantic-product callable result metadata: /main");
}

TEST_CASE("ir lowerer rejects missing semantic-product callable summary path id") {
  primec::Program program;

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 8201;
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  semanticProgram.callableSummaries.push_back(primec::SemanticProgramCallableSummary{
      .isExecution = false,
      .returnKind = "return",
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
      .semanticNodeId = 8201,
      .provenanceHandle = 0,
      .fullPathId = primec::InvalidSymbolId,
  });
  semanticProgram.returnFacts.push_back(primec::SemanticProgramReturnFact{
      .returnKind = "return",
      .structPath = "",
      .bindingTypeText = "i32",
      .isMutable = false,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 8201,
      .definitionPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
  });
  semanticProgram.publishedRoutingLookups.returnFactIndicesByDefinitionId
      .insert_or_assign(8201, semanticProgram.returnFacts.size() - 1);

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "missing semantic-product callable summary path id");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("ir lowerer rejects invalid semantic-product callable summary path id") {
  primec::Program program;

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 8202;
  primec::Expr returnExpr;
  returnExpr.kind = primec::Expr::Kind::Literal;
  returnExpr.literalValue = 1;
  returnExpr.intWidth = 32;
  mainDef.returnExpr = returnExpr;
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  semanticProgram.callableSummaries.push_back(primec::SemanticProgramCallableSummary{
      .isExecution = false,
      .returnKind = "return",
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
      .semanticNodeId = 8202,
      .provenanceHandle = 0,
      .fullPathId =
          static_cast<primec::SymbolId>(semanticProgram.callTargetStringTable.size() + 1u),
  });

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "missing semantic-product callable summary path id");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("ir lowerer rejects missing semantic-product return binding types") {
  primec::Program program;

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  semanticProgram.callableSummaries.push_back(primec::SemanticProgramCallableSummary{
      .isExecution = false,
      .returnKind = "return",
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
      .semanticNodeId = 0,
      .provenanceHandle = 0,
      .fullPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
  });
  semanticProgram.returnFacts.push_back(primec::SemanticProgramReturnFact{
      .returnKind = "return",
      .structPath = "",
      .bindingTypeText = "",
      .isMutable = false,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 0,
      .definitionPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
  });
  semanticProgram.publishedRoutingLookups.returnFactIndicesByDefinitionId
      .insert_or_assign(83020, semanticProgram.returnFacts.size() - 1);

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "missing semantic-product return binding type: /main");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("ir lowerer result completeness requires published return maps") {
  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  semanticProgram.callableSummaries.push_back(primec::SemanticProgramCallableSummary{
      .isExecution = false,
      .returnKind = "return",
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
      .semanticNodeId = 9401,
      .provenanceHandle = 0,
      .fullPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
  });
  semanticProgram.returnFacts.push_back(primec::SemanticProgramReturnFact{
      .returnKind = "return",
      .structPath = "",
      .bindingTypeText = "",
      .isMutable = false,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 9401,
      .definitionPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
  });

  std::string error;
  CHECK(primec::ir_lowerer::validateSemanticProductResultMetadataCompleteness(
      &semanticProgram, error));
  CHECK(error.empty());

  semanticProgram.publishedRoutingLookups.returnFactIndicesByDefinitionId
      .insert_or_assign(9401, 0);
  CHECK_FALSE(primec::ir_lowerer::validateSemanticProductResultMetadataCompleteness(
      &semanticProgram, error));
  CHECK(error == "missing semantic-product return binding type: /main");
}

TEST_CASE("ir lowerer rejects missing semantic-product return definition path id") {
  primec::Program program;

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 9101;
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  semanticProgram.callableSummaries.push_back(primec::SemanticProgramCallableSummary{
      .isExecution = false,
      .returnKind = "return",
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
      .semanticNodeId = 9101,
      .provenanceHandle = 0,
      .fullPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
  });
  semanticProgram.returnFacts.push_back(primec::SemanticProgramReturnFact{
      .returnKind = "return",
      .structPath = "",
      .bindingTypeText = "i32",
      .isMutable = false,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 9101,
      .definitionPathId = primec::InvalidSymbolId,
  });
  semanticProgram.publishedRoutingLookups.returnFactIndicesByDefinitionId
      .insert_or_assign(9101, semanticProgram.returnFacts.size() - 1);

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "missing semantic-product return definition path id");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("ir lowerer rejects stale semantic-product return facts") {
  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  semanticProgram.callableSummaries.push_back(primec::SemanticProgramCallableSummary{
      .isExecution = false,
      .returnKind = "i32",
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
      .semanticNodeId = 9102,
      .provenanceHandle = 0,
      .fullPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
      .returnKindId = primec::semanticProgramInternCallTargetString(semanticProgram, "i32"),
  });
  semanticProgram.returnFacts.push_back(primec::SemanticProgramReturnFact{
      .returnKind = "i32",
      .structPath = "/i32",
      .bindingTypeText = "",
      .isMutable = false,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 9102,
      .definitionPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
      .returnKindId = primec::semanticProgramInternCallTargetString(semanticProgram, "i32"),
      .structPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/i32"),
      .bindingTypeTextId = primec::semanticProgramInternCallTargetString(semanticProgram, "i32"),
  });
  semanticProgram.publishedRoutingLookups.returnFactIndicesByDefinitionId
      .insert_or_assign(9102, semanticProgram.returnFacts.size() - 1);
  publishFixtureCallableAndReturnRouting(semanticProgram);

  std::string error;
  CHECK(primec::ir_lowerer::validateSemanticProductResultMetadataCompleteness(
      &semanticProgram, error));
  CHECK(error.empty());

  semanticProgram.returnFacts.back().bindingTypeTextId =
      static_cast<primec::SymbolId>(semanticProgram.callTargetStringTable.size() + 1u);

  CHECK_FALSE(primec::ir_lowerer::validateSemanticProductResultMetadataCompleteness(
      &semanticProgram, error));
  CHECK(error == "missing semantic-product return binding type id: /main");

  semanticProgram.returnFacts.back().bindingTypeText = "i32";
  error.clear();
  CHECK_FALSE(primec::ir_lowerer::validateSemanticProductResultMetadataCompleteness(
      &semanticProgram, error));
  CHECK(error == "missing semantic-product return binding type id: /main");

  semanticProgram.returnFacts.back().bindingTypeTextId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "i64");
  error.clear();

  CHECK_FALSE(primec::ir_lowerer::validateSemanticProductResultMetadataCompleteness(
      &semanticProgram, error));
  CHECK(error == "stale semantic-product return binding type metadata: /main");

  semanticProgram.returnFacts.back().bindingTypeTextId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "i32");
  semanticProgram.returnFacts.back().structPathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/Other");
  error.clear();

  CHECK_FALSE(primec::ir_lowerer::validateSemanticProductResultMetadataCompleteness(
      &semanticProgram, error));
  CHECK(error == "stale semantic-product return struct path metadata: /main");

  semanticProgram.returnFacts.back().structPathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/i32");
  semanticProgram.returnFacts.back().returnKind = "return";
  semanticProgram.returnFacts.back().returnKindId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "return");
  error.clear();

  CHECK_FALSE(primec::ir_lowerer::validateSemanticProductResultMetadataCompleteness(
      &semanticProgram, error));
  CHECK(error == "stale semantic-product return fact: /main");
}

TEST_CASE("ir lowerer rejects incomplete semantic-product query facts") {
  primec::Program program;

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 83020;
  primec::Transform queryReturnTransform;
  queryReturnTransform.name = "return";
  queryReturnTransform.templateArgs.push_back("i32");
  mainDef.transforms.push_back(queryReturnTransform);
  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "lookup";
  callExpr.semanticNodeId = 83;
  mainDef.statements.push_back(callExpr);
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      .scopePath = "/main",
      .callName = "lookup",
      .sourceLine = 1,
      .sourceColumn = 1,
      .semanticNodeId = 83,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/lookup"),
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
      .resultValueType = "",
      .resultErrorType = "FileError",
      .sourceLine = 1,
      .sourceColumn = 1,
      .semanticNodeId = 83,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/lookup"),
  });
  semanticProgram.publishedRoutingLookups.queryFactIndicesByExpr.insert_or_assign(83, 0);
  primec::SemanticProgramCallableSummary callableSummary;
  callableSummary.semanticNodeId = 83020;
  callableSummary.fullPathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/main");
  callableSummary.returnKind = "i32";
  semanticProgram.callableSummaries.push_back(std::move(callableSummary));
  semanticProgram.returnFacts.push_back(primec::SemanticProgramReturnFact{
      .returnKind = "i32",
      .structPath = "",
      .bindingTypeText = "i32",
      .isMutable = false,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 83020,
      .provenanceHandle = 0,
      .definitionPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
  });
  semanticProgram.publishedRoutingLookups.returnFactIndicesByDefinitionId
      .insert_or_assign(83020, semanticProgram.returnFacts.size() - 1);

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "incomplete semantic-product query fact: lookup");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("ir lowerer rejects missing semantic-product query resolved path id") {
  primec::Program program;

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "lookup";
  callExpr.semanticNodeId = 8301;
  mainDef.statements.push_back(callExpr);
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      .scopePath = "/main",
      .callName = "lookup",
      .sourceLine = 1,
      .sourceColumn = 1,
      .semanticNodeId = 8301,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/lookup"),
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
      .sourceLine = 1,
      .sourceColumn = 1,
      .semanticNodeId = 8301,
      .resolvedPathId = primec::InvalidSymbolId,
  });
  semanticProgram.publishedRoutingLookups.queryFactIndicesByExpr.insert_or_assign(8301, 0);
  primec::SemanticProgramCallableSummary callableSummary;
  callableSummary.fullPathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/main");
  callableSummary.returnKind = "i32";
  semanticProgram.callableSummaries.push_back(std::move(callableSummary));
  semanticProgram.returnFacts.push_back(primec::SemanticProgramReturnFact{
      .returnKind = "i32",
      .structPath = "",
      .bindingTypeText = "i32",
      .isMutable = false,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 0,
      .provenanceHandle = 0,
      .definitionPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
  });

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "missing semantic-product query resolved path id: lookup");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("ir lowerer rejects stale semantic-product query facts") {
  primec::Program program;

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 83020;
  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "plus";
  callExpr.semanticNodeId = 8302;
  primec::Expr leftArg;
  leftArg.kind = primec::Expr::Kind::Literal;
  leftArg.literalValue = 1;
  leftArg.intWidth = 32;
  primec::Expr rightArg;
  rightArg.kind = primec::Expr::Kind::Literal;
  rightArg.literalValue = 2;
  rightArg.intWidth = 32;
  callExpr.args.push_back(leftArg);
  callExpr.args.push_back(rightArg);
  mainDef.returnExpr = callExpr;
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      .scopePath = "/main",
      .callName = "plus",
      .sourceLine = 1,
      .sourceColumn = 1,
      .semanticNodeId = 8302,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/plus"),
      .stdlibSurfaceId = std::nullopt,
  });
  semanticProgram.publishedRoutingLookups.directCallTargetIdsByExpr.insert_or_assign(
      8302, semanticProgram.directCallTargets.back().resolvedPathId);
  semanticProgram.queryFacts.push_back(primec::SemanticProgramQueryFact{
      .scopePath = "/main",
      .callName = "plus",
      .queryTypeText = "Result<i32, FileError>",
      .bindingTypeText = "Result<i32, FileError>",
      .receiverBindingTypeText = "",
      .hasResultType = true,
      .resultTypeHasValue = true,
      .resultValueType = "i32",
      .resultErrorType = "FileError",
      .sourceLine = 1,
      .sourceColumn = 1,
      .semanticNodeId = 8302,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/stale_lookup"),
  });
  semanticProgram.publishedRoutingLookups.queryFactIndicesByExpr.insert_or_assign(8302, 0);
  primec::SemanticProgramCallableSummary callableSummary;
  callableSummary.semanticNodeId = 83020;
  callableSummary.fullPathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/main");
  callableSummary.returnKind = "i32";
  const auto callableSummaryFullPathId = callableSummary.fullPathId;
  semanticProgram.callableSummaries.push_back(std::move(callableSummary));
  semanticProgram.publishedRoutingLookups.callableSummaryIndicesByPathId.insert_or_assign(
      callableSummaryFullPathId, static_cast<std::size_t>(0));
  semanticProgram.returnFacts.push_back(primec::SemanticProgramReturnFact{
      .returnKind = "i32",
      .structPath = "",
      .bindingTypeText = "i32",
      .isMutable = false,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 83020,
      .provenanceHandle = 0,
      .definitionPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
  });
  semanticProgram.publishedRoutingLookups.returnFactIndicesByDefinitionId.insert_or_assign(
      static_cast<uint64_t>(83020), static_cast<std::size_t>(0));

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "stale semantic-product query fact: plus");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("ir lowerer accepts query-owned builtin count target metadata") {
  const auto acceptsCountShadow = [](const char *publishedPath) {
    primec::SemanticProgram semanticProgram;
    semanticProgram.entryPath = "/main";
    semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
        .scopePath = "/main",
        .callName = "count",
        .sourceLine = 1,
        .sourceColumn = 1,
        .semanticNodeId = 8303,
        .resolvedPathId =
            primec::semanticProgramInternCallTargetString(semanticProgram, publishedPath),
        .stdlibSurfaceId = std::nullopt,
    });
    semanticProgram.queryFacts.push_back(primec::SemanticProgramQueryFact{
        .scopePath = "/main",
        .callName = "count",
        .queryTypeText = "i32",
        .bindingTypeText = "i32",
        .receiverBindingTypeText = "",
        .hasResultType = false,
        .resultTypeHasValue = false,
        .resultValueType = "",
        .resultErrorType = "",
        .sourceLine = 1,
        .sourceColumn = 1,
        .semanticNodeId = 8303,
        .callNameId = primec::semanticProgramInternCallTargetString(semanticProgram, "count"),
        .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/count"),
        .queryTypeTextId = primec::semanticProgramInternCallTargetString(semanticProgram, "i32"),
        .bindingTypeTextId = primec::semanticProgramInternCallTargetString(semanticProgram, "i32"),
        .receiverBindingTypeTextId =
            primec::semanticProgramInternCallTargetString(semanticProgram, ""),
    });
    semanticProgram.publishedRoutingLookups.queryFactIndicesByExpr.insert_or_assign(8303, 0);

    std::string error;
    CHECK(primec::ir_lowerer::validateSemanticProductResultMetadataCompleteness(
        &semanticProgram, error));
    CHECK(error.empty());
  };

  acceptsCountShadow("/array/count");
  acceptsCountShadow("/string/count");
}

TEST_CASE("ir lowerer rejects stale semantic-product query type metadata") {
  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      .scopePath = "/main",
      .callName = "lookup",
      .sourceLine = 1,
      .sourceColumn = 1,
      .semanticNodeId = 8304,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/lookup"),
      .stdlibSurfaceId = std::nullopt,
  });
  semanticProgram.queryFacts.push_back(primec::SemanticProgramQueryFact{
      .scopePath = "/main",
      .callName = "lookup",
      .queryTypeText = "i32",
      .bindingTypeText = "i32",
      .receiverBindingTypeText = "Map<string, i32>",
      .hasResultType = false,
      .resultTypeHasValue = false,
      .resultValueType = "",
      .resultErrorType = "",
      .sourceLine = 1,
      .sourceColumn = 1,
      .semanticNodeId = 8304,
      .callNameId = primec::semanticProgramInternCallTargetString(semanticProgram, "lookup"),
      .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/lookup"),
      .queryTypeTextId = primec::semanticProgramInternCallTargetString(semanticProgram, "i32"),
      .bindingTypeTextId = primec::semanticProgramInternCallTargetString(semanticProgram, "i32"),
      .receiverBindingTypeTextId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "Map<string, i32>"),
  });
  semanticProgram.publishedRoutingLookups.queryFactIndicesByExpr.insert_or_assign(8304, 0);

  std::string error;
  CHECK(primec::ir_lowerer::validateSemanticProductResultMetadataCompleteness(
      &semanticProgram, error));
  CHECK(error.empty());

  semanticProgram.queryFacts.back().queryTypeTextId =
      static_cast<primec::SymbolId>(semanticProgram.callTargetStringTable.size() + 1u);

  CHECK_FALSE(primec::ir_lowerer::validateSemanticProductResultMetadataCompleteness(
      &semanticProgram, error));
  CHECK(error == "missing semantic-product query type id: lookup");

  semanticProgram.queryFacts.back().queryTypeTextId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "i64");
  error.clear();

  CHECK_FALSE(primec::ir_lowerer::validateSemanticProductResultMetadataCompleteness(
      &semanticProgram, error));
  CHECK(error == "stale semantic-product query type metadata: lookup");

  semanticProgram.queryFacts.back().queryTypeTextId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "i32");
  semanticProgram.queryFacts.back().bindingTypeTextId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "i64");
  error.clear();

  CHECK_FALSE(primec::ir_lowerer::validateSemanticProductResultMetadataCompleteness(
      &semanticProgram, error));
  CHECK(error == "stale semantic-product query binding type metadata: lookup");

  semanticProgram.queryFacts.back().bindingTypeTextId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "i32");
  semanticProgram.queryFacts.back().receiverBindingTypeTextId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "OtherMap");
  error.clear();

  CHECK_FALSE(primec::ir_lowerer::validateSemanticProductResultMetadataCompleteness(
      &semanticProgram, error));
  CHECK(error == "stale semantic-product query receiver binding type metadata: lookup");
}

TEST_SUITE_END();
