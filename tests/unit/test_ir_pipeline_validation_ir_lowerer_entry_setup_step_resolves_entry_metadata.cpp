#include "test_ir_pipeline_validation_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("ir lowerer entry setup step resolves entry metadata") {
  primec::Program program;
  primec::Definition entryDef;
  entryDef.fullPath = "/main";
  program.definitions.push_back(entryDef);

  const primec::Definition *resolvedEntry = nullptr;
  uint64_t entryEffectMask = 0;
  uint64_t entryCapabilityMask = 0;
  std::string error;
  CHECK(primec::ir_lowerer::runLowerEntrySetup(program,
                                               nullptr,
                                               "/main",
                                               {"io_out"},
                                               {"io_err"},
                                               primec::IrValidationTarget::Native,
                                               resolvedEntry,
                                               entryEffectMask,
                                               entryCapabilityMask,
                                               error));
  CHECK(error.empty());
  REQUIRE(resolvedEntry != nullptr);
  CHECK(resolvedEntry->fullPath == "/main");
  CHECK(entryEffectMask == primec::EffectIoErr);
  CHECK(entryCapabilityMask == primec::EffectIoErr);
}

TEST_CASE("ir lowerer entry setup step rejects missing entry") {
  primec::Program program;
  primec::Definition helperDef;
  helperDef.fullPath = "/helper";
  program.definitions.push_back(helperDef);

  const primec::Definition *resolvedEntry = nullptr;
  uint64_t entryEffectMask = 123;
  uint64_t entryCapabilityMask = 456;
  std::string error;
  CHECK_FALSE(primec::ir_lowerer::runLowerEntrySetup(program,
                                                     nullptr,
                                                     "/main",
                                                     {"io_out"},
                                                     {"io_err"},
                                                     primec::IrValidationTarget::Native,
                                                     resolvedEntry,
                                                     entryEffectMask,
                                                     entryCapabilityMask,
                                                     error));
  CHECK(error == "native backend requires entry definition /main");
  CHECK(resolvedEntry == nullptr);
  CHECK(entryEffectMask == 0);
  CHECK(entryCapabilityMask == 0);
}

TEST_CASE("ir lowerer entry setup step rejects published software numeric preflight facts") {
  primec::Program program;
  primec::Definition entryDef;
  entryDef.fullPath = "/main";
  program.definitions.push_back(entryDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  semanticProgram.publishedLowererPreflightFacts.firstSoftwareNumericTypeId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "decimal");

  const primec::Definition *resolvedEntry = nullptr;
  uint64_t entryEffectMask = 0;
  uint64_t entryCapabilityMask = 0;
  std::string error;
  CHECK_FALSE(primec::ir_lowerer::runLowerEntrySetup(program,
                                                     &semanticProgram,
                                                     "/main",
                                                     {"io_out"},
                                                     {"io_err"},
                                                     primec::IrValidationTarget::Native,
                                                     resolvedEntry,
                                                     entryEffectMask,
                                                     entryCapabilityMask,
                                                     error));
  CHECK(error == "native backend does not support software numeric types: decimal");
}

TEST_CASE("ir lowerer entry setup step rejects published runtime reflection preflight facts") {
  primec::Program program;
  primec::Definition entryDef;
  entryDef.fullPath = "/main";
  program.definitions.push_back(entryDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  semanticProgram.publishedLowererPreflightFacts.firstRuntimeReflectionPathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/meta/type_name");

  const primec::Definition *resolvedEntry = nullptr;
  uint64_t entryEffectMask = 0;
  uint64_t entryCapabilityMask = 0;
  std::string error;
  CHECK_FALSE(primec::ir_lowerer::runLowerEntrySetup(program,
                                                     &semanticProgram,
                                                     "/main",
                                                     {"io_out"},
                                                     {"io_err"},
                                                     primec::IrValidationTarget::Native,
                                                     resolvedEntry,
                                                     entryEffectMask,
                                                     entryCapabilityMask,
                                                     error));
  CHECK(error ==
        "native backend requires compile-time reflection query elimination before IR emission: /meta/type_name");
}

TEST_CASE("ir lowerer entry setup step defers routing completeness to conformance coverage") {
  primec::Program program;
  primec::Definition entryDef;
  entryDef.fullPath = "/main";

  primec::Expr directCallExpr;
  directCallExpr.kind = primec::Expr::Kind::Call;
  directCallExpr.name = "callee";
  directCallExpr.semanticNodeId = 41;
  entryDef.statements.push_back(directCallExpr);

  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "values";
  primec::Expr methodCallExpr;
  methodCallExpr.kind = primec::Expr::Kind::Call;
  methodCallExpr.name = "count";
  methodCallExpr.isMethodCall = true;
  methodCallExpr.semanticNodeId = 42;
  methodCallExpr.args.push_back(receiverExpr);
  entryDef.statements.push_back(methodCallExpr);

  primec::Expr bridgeCallExpr;
  bridgeCallExpr.kind = primec::Expr::Kind::Call;
  bridgeCallExpr.name = "/std/collections/mapContains";
  bridgeCallExpr.semanticNodeId = 43;
  entryDef.statements.push_back(bridgeCallExpr);

  program.definitions.push_back(entryDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  primec::SemanticProgramCallableSummary entrySummary;
  entrySummary.fullPathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/main");
  semanticProgram.callableSummaries.push_back(entrySummary);

  const primec::Definition *resolvedEntry = nullptr;
  uint64_t entryEffectMask = 0;
  uint64_t entryCapabilityMask = 0;
  std::string error;
  CHECK(primec::ir_lowerer::runLowerEntrySetup(program,
                                               &semanticProgram,
                                               "/main",
                                               {"io_out"},
                                               {"io_err"},
                                               primec::IrValidationTarget::Native,
                                               resolvedEntry,
                                               entryEffectMask,
                                               entryCapabilityMask,
                                               error));
  CHECK(error.empty());
  REQUIRE(resolvedEntry != nullptr);
  CHECK(resolvedEntry->fullPath == "/main");
}

TEST_CASE("ir lowerer imports structs setup step builds maps and layouts") {
  primec::Program program;

  primec::Definition structDef;
  structDef.fullPath = "/Vec2";
  primec::Transform structTransform;
  structTransform.name = "struct";
  structDef.transforms.push_back(structTransform);

  primec::Expr xField;
  xField.kind = primec::Expr::Kind::Name;
  xField.isBinding = true;
  xField.name = "x";
  primec::Transform xType;
  xType.name = "i32";
  xField.transforms.push_back(xType);
  structDef.statements.push_back(xField);

  primec::Expr yField;
  yField.kind = primec::Expr::Kind::Name;
  yField.isBinding = true;
  yField.name = "y";
  primec::Transform yType;
  yType.name = "i32";
  yField.transforms.push_back(yType);
  structDef.statements.push_back(yField);
  program.definitions.push_back(structDef);

  primec::Definition entryDef;
  entryDef.fullPath = "/main";
  program.definitions.push_back(entryDef);

  primec::IrModule module;
  std::unordered_map<std::string, const primec::Definition *> defMap;
  std::unordered_set<std::string> structNames;
  std::unordered_map<std::string, std::string> importAliases;
  std::unordered_map<std::string, std::vector<primec::ir_lowerer::LayoutFieldBinding>> structFieldInfoByName;
  std::string error;

  CHECK(primec::ir_lowerer::runLowerImportsStructsSetup(
      program, module, defMap, structNames, importAliases, structFieldInfoByName, error));
  CHECK(error.empty());
  CHECK(defMap.find("/Vec2") != defMap.end());
  CHECK(defMap.find("/main") != defMap.end());
  CHECK(structNames.count("/Vec2") == 1);
  CHECK(importAliases.empty());
  CHECK(structFieldInfoByName.count("/Vec2") == 1);
  CHECK(structFieldInfoByName.at("/Vec2").size() == 2);
  CHECK(module.structLayouts.size() == 1);
  CHECK(module.structLayouts.front().name == "/Vec2");
}

TEST_CASE("ir lowerer imports structs setup step rejects unknown field envelopes") {
  primec::Program program;
  primec::Definition structDef;
  structDef.fullPath = "/Broken";
  primec::Transform structTransform;
  structTransform.name = "struct";
  structDef.transforms.push_back(structTransform);

  primec::Expr badField;
  badField.kind = primec::Expr::Kind::Name;
  badField.isBinding = true;
  badField.name = "value";
  primec::Transform badType;
  badType.name = "UnknownEnvelope";
  badField.transforms.push_back(badType);
  structDef.statements.push_back(badField);

  program.definitions.push_back(structDef);

  primec::IrModule module;
  std::unordered_map<std::string, const primec::Definition *> defMap;
  std::unordered_set<std::string> structNames;
  std::unordered_map<std::string, std::string> importAliases;
  std::unordered_map<std::string, std::vector<primec::ir_lowerer::LayoutFieldBinding>> structFieldInfoByName;
  std::string error;

  CHECK_FALSE(primec::ir_lowerer::runLowerImportsStructsSetup(
      program, module, defMap, structNames, importAliases, structFieldInfoByName, error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("ir lowerer locals setup step resolves orchestration adapters") {
  primec::Program program;
  primec::Definition entryDef;
  entryDef.fullPath = "/main";
  program.definitions.push_back(entryDef);

  primec::IrModule module;
  std::unordered_map<std::string, const primec::Definition *> defMap;
  std::unordered_set<std::string> structNames;
  std::unordered_map<std::string, std::string> importAliases;
  std::unordered_map<std::string, std::vector<primec::ir_lowerer::LayoutFieldBinding>> structFieldInfoByName;
  std::string error;

  REQUIRE(primec::ir_lowerer::runLowerImportsStructsSetup(
      program, module, defMap, structNames, importAliases, structFieldInfoByName, error));
  REQUIRE(error.empty());

  primec::IrFunction function;
  function.name = "/main";
  std::vector<std::string> stringTable;
  primec::ir_lowerer::SetupLocalsOrchestration setupLocalsOrchestration;
  CHECK(primec::ir_lowerer::runLowerLocalsSetup(stringTable,
                                                function,
                                                program,
                                                program.definitions.front(),
                                                "/main",
                                                defMap,
                                                importAliases,
                                                structNames,
                                                structFieldInfoByName,
                                                setupLocalsOrchestration,
                                                error));
  CHECK(error.empty());
  CHECK(setupLocalsOrchestration.entryReturnConfig.returnsVoid);
  CHECK(static_cast<bool>(
      setupLocalsOrchestration.runtimeErrorAndStringLiteralSetup.stringLiteralHelpers.internString));
  CHECK(static_cast<bool>(setupLocalsOrchestration.entryCountAccessSetup.classifiers.isEntryArgsName));
  CHECK(static_cast<bool>(setupLocalsOrchestration.entryCallOnErrorSetup.callResolutionAdapters.resolveExprPath));
  CHECK(static_cast<bool>(setupLocalsOrchestration.setupMathResolvers.getMathBuiltinName));
  CHECK(static_cast<bool>(setupLocalsOrchestration.bindingTypeAdapters.bindingKind));
  CHECK(static_cast<bool>(setupLocalsOrchestration.setupTypeAndStructTypeAdapters.combineNumericKinds));
  CHECK(static_cast<bool>(setupLocalsOrchestration.applyStructValueInfo));
}

TEST_CASE("ir lowerer locals setup step rejects unsupported string array return") {
  primec::Program program;
  primec::Definition entryDef;
  entryDef.fullPath = "/main";
  primec::Transform returnTransform;
  returnTransform.name = "return";
  returnTransform.templateArgs.push_back("array<string>");
  entryDef.transforms.push_back(returnTransform);
  program.definitions.push_back(entryDef);

  primec::IrModule module;
  std::unordered_map<std::string, const primec::Definition *> defMap;
  std::unordered_set<std::string> structNames;
  std::unordered_map<std::string, std::string> importAliases;
  std::unordered_map<std::string, std::vector<primec::ir_lowerer::LayoutFieldBinding>> structFieldInfoByName;
  std::string error;

  REQUIRE(primec::ir_lowerer::runLowerImportsStructsSetup(
      program, module, defMap, structNames, importAliases, structFieldInfoByName, error));
  REQUIRE(error.empty());

  primec::IrFunction function;
  function.name = "/main";
  std::vector<std::string> stringTable;
  primec::ir_lowerer::SetupLocalsOrchestration setupLocalsOrchestration;
  CHECK_FALSE(primec::ir_lowerer::runLowerLocalsSetup(stringTable,
                                                      function,
                                                      program,
                                                      program.definitions.front(),
                                                      "/main",
                                                      defMap,
                                                      importAliases,
                                                      structNames,
                                                      structFieldInfoByName,
                                                      setupLocalsOrchestration,
                                                      error));
  CHECK(error == "native backend does not support string array return types on /main");
  CHECK_FALSE(setupLocalsOrchestration.entryReturnConfig.hasReturnTransform);
}

TEST_CASE("ir lowerer inference setup bootstrap wires callbacks") {
  std::unordered_map<std::string, const primec::Definition *> defMap;
  std::unordered_map<std::string, std::string> importAliases;
  std::unordered_set<std::string> structNames;

  primec::ir_lowerer::LowerInferenceSetupBootstrapState state;
  std::string error;
  CHECK(primec::ir_lowerer::runLowerInferenceSetupBootstrap(
      {
          .defMap = &defMap,
          .importAliases = &importAliases,
          .structNames = &structNames,
          .isArrayCountCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
          .isVectorCapacityCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
          .isEntryArgsName = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
          .resolveExprPath = [](const primec::Expr &) { return std::string(); },
          .getBuiltinOperatorName = [](const primec::Expr &, std::string &) { return false; },
      },
      state,
      error));
  CHECK(error.empty());
  CHECK(state.returnInfoCache.empty());
  CHECK(state.returnInferenceStack.empty());
  CHECK_FALSE(static_cast<bool>(state.getReturnInfo));
  CHECK(static_cast<bool>(state.resolveMethodCallDefinition));
  CHECK(static_cast<bool>(state.inferPointerTargetKind));

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo pointerInfo;
  pointerInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Pointer;
  pointerInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  locals.emplace("ptr", pointerInfo);

  primec::Expr pointerExpr;
  pointerExpr.kind = primec::Expr::Kind::Name;
  pointerExpr.name = "ptr";
  CHECK(state.inferPointerTargetKind(pointerExpr, locals) == primec::ir_lowerer::LocalInfo::ValueKind::Int64);
}

TEST_CASE("ir lowerer inference setup bootstrap validates dependencies") {
  std::unordered_map<std::string, const primec::Definition *> defMap;
  std::unordered_map<std::string, std::string> importAliases;
  std::unordered_set<std::string> structNames;

  primec::ir_lowerer::LowerInferenceSetupBootstrapState state;
  std::string error;
  CHECK_FALSE(primec::ir_lowerer::runLowerInferenceSetupBootstrap(
      {
          .defMap = &defMap,
          .importAliases = &importAliases,
          .structNames = &structNames,
          .isArrayCountCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
          .isVectorCapacityCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
          .isEntryArgsName = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
          .resolveExprPath = {},
          .getBuiltinOperatorName = [](const primec::Expr &, std::string &) { return false; },
      },
      state,
      error));
  CHECK(error == "native backend missing inference setup bootstrap dependency: resolveExprPath");
  CHECK_FALSE(static_cast<bool>(state.resolveMethodCallDefinition));
}

TEST_CASE("ir lowerer inference setup orchestrates callbacks") {
  primec::Program program;
  program.definitions.emplace_back();
  primec::Definition &callee = program.definitions.back();
  callee.fullPath = "/callee";
  std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/callee", &callee},
  };
  std::unordered_map<std::string, std::string> importAliases;
  std::unordered_set<std::string> structNames;

  primec::ir_lowerer::LowerInferenceSetupBootstrapState state;
  std::string error;
  CHECK(primec::ir_lowerer::runLowerInferenceSetup(
      {
          .program = &program,
          .defMap = &defMap,
          .importAliases = &importAliases,
          .structNames = &structNames,
          .isArrayCountCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
          .isStringCountCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
          .isVectorCapacityCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
          .isEntryArgsName = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
          .resolveExprPath = [](const primec::Expr &) { return std::string(); },
          .getBuiltinOperatorName = [](const primec::Expr &, std::string &) { return false; },
          .resolveStructArrayInfoFromPath =
              [](const std::string &, primec::ir_lowerer::StructArrayTypeInfo &) { return false; },
          .inferStructExprPath =
              [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string(); },
          .resolveStructFieldSlot =
              [](const std::string &, const std::string &, primec::ir_lowerer::StructSlotFieldInfo &) {
                return false;
              },
          .resolveUninitializedStorage =
              [](const primec::Expr &,
                 const primec::ir_lowerer::LocalMap &,
                 primec::ir_lowerer::UninitializedStorageAccessInfo &,
                 bool &resolved) {
                resolved = false;
                return true;
              },
          .hasMathImport = false,
          .combineNumericKinds =
              [](primec::ir_lowerer::LocalInfo::ValueKind, primec::ir_lowerer::LocalInfo::ValueKind) {
                return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
              },
          .getMathConstantName = [](const std::string &, std::string &) { return false; },
          .resolveStructTypeName = [](const std::string &, const std::string &, std::string &) { return false; },
          .isBindingMutable = [](const primec::Expr &) { return false; },
          .bindingKind = [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::Kind::Value; },
          .hasExplicitBindingTypeTransform = [](const primec::Expr &) { return false; },
          .bindingValueKind = [](const primec::Expr &, primec::ir_lowerer::LocalInfo::Kind) {
            return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
          },
          .isFileErrorBinding = [](const primec::Expr &) { return false; },
          .setReferenceArrayInfo = [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
          .applyStructArrayInfo = [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
          .applyStructValueInfo = [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
          .isStringBinding = [](const primec::Expr &) { return false; },
          .lowerMatchToIf = [](const primec::Expr &expr, primec::Expr &expandedExpr, std::string &) {
            expandedExpr = expr;
            return true;
          },
      },
      state,
      error));
  CHECK(error.empty());
  CHECK(static_cast<bool>(state.getReturnInfo));
  CHECK(static_cast<bool>(state.inferExprKind));
  CHECK(static_cast<bool>(state.inferArrayElementKind));
  CHECK(static_cast<bool>(state.resolveMethodCallDefinition));

  primec::ir_lowerer::ReturnInfo returnInfo;
  CHECK(state.getReturnInfo("/callee", returnInfo));
  CHECK(returnInfo.returnsVoid);

  primec::Expr literalExpr;
  literalExpr.kind = primec::Expr::Kind::Literal;
  CHECK(state.inferExprKind(literalExpr, primec::ir_lowerer::LocalMap{}) ==
        primec::ir_lowerer::LocalInfo::ValueKind::Int32);
}

TEST_CASE("ir lowerer inference setup validates dependencies") {
  std::unordered_map<std::string, const primec::Definition *> defMap;
  std::unordered_map<std::string, std::string> importAliases;
  std::unordered_set<std::string> structNames;

  primec::ir_lowerer::LowerInferenceSetupBootstrapState state;
  std::string error;
  CHECK_FALSE(primec::ir_lowerer::runLowerInferenceSetup(
      {
          .defMap = &defMap,
          .importAliases = &importAliases,
          .structNames = &structNames,
          .isArrayCountCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
          .isStringCountCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
          .isVectorCapacityCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
          .isEntryArgsName = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
          .resolveExprPath = {},
          .getBuiltinOperatorName = [](const primec::Expr &, std::string &) { return false; },
          .resolveStructArrayInfoFromPath =
              [](const std::string &, primec::ir_lowerer::StructArrayTypeInfo &) { return false; },
          .inferStructExprPath =
              [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string(); },
          .resolveStructFieldSlot =
              [](const std::string &, const std::string &, primec::ir_lowerer::StructSlotFieldInfo &) {
                return false;
              },
          .resolveUninitializedStorage =
              [](const primec::Expr &,
                 const primec::ir_lowerer::LocalMap &,
                 primec::ir_lowerer::UninitializedStorageAccessInfo &,
                 bool &resolved) {
                resolved = false;
                return true;
              },
          .hasMathImport = false,
          .combineNumericKinds =
              [](primec::ir_lowerer::LocalInfo::ValueKind, primec::ir_lowerer::LocalInfo::ValueKind) {
                return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
              },
          .getMathConstantName = [](const std::string &, std::string &) { return false; },
          .resolveStructTypeName = [](const std::string &, const std::string &, std::string &) { return false; },
          .isBindingMutable = [](const primec::Expr &) { return false; },
          .bindingKind = [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::Kind::Value; },
          .hasExplicitBindingTypeTransform = [](const primec::Expr &) { return false; },
          .bindingValueKind = [](const primec::Expr &, primec::ir_lowerer::LocalInfo::Kind) {
            return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
          },
          .isFileErrorBinding = [](const primec::Expr &) { return false; },
          .setReferenceArrayInfo = [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
          .applyStructArrayInfo = [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
          .applyStructValueInfo = [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
          .isStringBinding = [](const primec::Expr &) { return false; },
          .lowerMatchToIf = [](const primec::Expr &expr, primec::Expr &expandedExpr, std::string &) {
            expandedExpr = expr;
            return true;
          },
      },
      state,
      error));
  CHECK(error == "native backend missing inference setup bootstrap dependency: resolveExprPath");
  CHECK_FALSE(static_cast<bool>(state.getReturnInfo));
}

TEST_CASE("ir lowerer inference array-kind setup wires callbacks") {
  std::unordered_map<std::string, const primec::Definition *> defMap;
  std::unordered_map<std::string, std::string> importAliases;
  std::unordered_set<std::string> structNames;

  primec::ir_lowerer::LowerInferenceSetupBootstrapState state;
  std::string error;
  REQUIRE(primec::ir_lowerer::runLowerInferenceSetupBootstrap(
      {
          .defMap = &defMap,
          .importAliases = &importAliases,
          .structNames = &structNames,
          .isArrayCountCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
          .isVectorCapacityCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
          .isEntryArgsName = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
          .resolveExprPath = [](const primec::Expr &) { return std::string(); },
          .getBuiltinOperatorName = [](const primec::Expr &, std::string &) { return false; },
      },
      state,
      error));
  REQUIRE(error.empty());

  CHECK(primec::ir_lowerer::runLowerInferenceArrayKindSetup(
      {
          .defMap = &defMap,
          .resolveExprPath = [](const primec::Expr &) { return std::string(); },
          .resolveStructArrayInfoFromPath =
              [](const std::string &, primec::ir_lowerer::StructArrayTypeInfo &) { return false; },
          .isArrayCountCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
          .isStringCountCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      },
      state,
      error));
  CHECK(error.empty());
  CHECK(static_cast<bool>(state.inferBufferElementKind));
  CHECK(static_cast<bool>(state.inferArrayElementKind));

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo bufferInfo;
  bufferInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Buffer;
  bufferInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Float64;
  locals.emplace("buf", bufferInfo);
  primec::Expr bufferExpr;
  bufferExpr.kind = primec::Expr::Kind::Name;
  bufferExpr.name = "buf";
  CHECK(state.inferBufferElementKind(bufferExpr, locals) == primec::ir_lowerer::LocalInfo::ValueKind::Float64);

  primec::ir_lowerer::LocalInfo arrayInfo;
  arrayInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  arrayInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  locals.emplace("arr", arrayInfo);
  primec::Expr arrayExpr;
  arrayExpr.kind = primec::Expr::Kind::Name;
  arrayExpr.name = "arr";
  CHECK(state.inferArrayElementKind(arrayExpr, locals) == primec::ir_lowerer::LocalInfo::ValueKind::Int64);
}

TEST_CASE("ir lowerer inference array-kind setup validates dependencies") {
  primec::ir_lowerer::LowerInferenceSetupBootstrapState state;
  std::unordered_map<std::string, const primec::Definition *> defMap;

  std::string error;
  CHECK_FALSE(primec::ir_lowerer::runLowerInferenceArrayKindSetup(
      {
          .defMap = &defMap,
          .resolveExprPath = [](const primec::Expr &) { return std::string(); },
          .resolveStructArrayInfoFromPath = {},
          .isArrayCountCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
          .isStringCountCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      },
      state,
      error));
  CHECK(error == "native backend missing inference array-kind setup dependency: resolveStructArrayInfoFromPath");
}

TEST_CASE("ir lowerer inference expr-kind base setup wires callback") {
  primec::ir_lowerer::LowerInferenceSetupBootstrapState state;
  std::string error;
  CHECK(primec::ir_lowerer::runLowerInferenceExprKindBaseSetup(
      {
          .getMathConstantName = [](const std::string &name, std::string &out) {
            if (name == "PI") {
              out = "PI";
              return true;
            }
            return false;
          },
      },
      state,
      error));
  CHECK(error.empty());
  CHECK(static_cast<bool>(state.inferLiteralOrNameExprKind));

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo refArrayInfo;
  refArrayInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Reference;
  refArrayInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  refArrayInfo.referenceToArray = true;
  locals.emplace("refArray", refArrayInfo);

  primec::Expr piNameExpr;
  piNameExpr.kind = primec::Expr::Kind::Name;
  piNameExpr.name = "PI";
  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(state.inferLiteralOrNameExprKind(piNameExpr, locals, kindOut));
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Float64);

  primec::Expr refArrayNameExpr;
  refArrayNameExpr.kind = primec::Expr::Kind::Name;
  refArrayNameExpr.name = "refArray";
  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(state.inferLiteralOrNameExprKind(refArrayNameExpr, locals, kindOut));
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);

  primec::Expr literalExpr;
  literalExpr.kind = primec::Expr::Kind::Literal;
  literalExpr.isUnsigned = true;
  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(state.inferLiteralOrNameExprKind(literalExpr, locals, kindOut));
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::UInt64);
}

TEST_CASE("ir lowerer inference expr-kind base setup validates dependencies") {
  primec::ir_lowerer::LowerInferenceSetupBootstrapState state;
  std::string error;
  CHECK_FALSE(primec::ir_lowerer::runLowerInferenceExprKindBaseSetup(
      {
          .getMathConstantName = {},
      },
      state,
      error));
  CHECK(error == "native backend missing inference expr-kind base setup dependency: getMathConstantName");
}

TEST_CASE("ir lowerer inference expr-kind call-base setup wires callback") {
  primec::ir_lowerer::LowerInferenceSetupBootstrapState state;
  std::string error;
  CHECK(primec::ir_lowerer::runLowerInferenceExprKindCallBaseSetup(
      {
          .inferStructExprPath = [](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &) {
            return expr.name == "receiver" ? std::string("/Struct") : std::string();
          },
          .resolveStructFieldSlot =
              [](const std::string &structPath, const std::string &fieldName, primec::ir_lowerer::StructSlotFieldInfo &out) {
                if (structPath == "/Struct" && fieldName == "value") {
                  out.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Float32;
                  out.structPath.clear();
                  return true;
                }
                return false;
              },
          .resolveUninitializedStorage =
              [](const primec::Expr &storageExpr,
                 const primec::ir_lowerer::LocalMap &,
                 primec::ir_lowerer::UninitializedStorageAccessInfo &out,
                 bool &resolved) {
                resolved = (storageExpr.kind == primec::Expr::Kind::Name && storageExpr.name == "slot");
                if (resolved) {
                  out.typeInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
                  out.typeInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
                  out.typeInfo.structPath.clear();
                }
                return true;
              },
      },
      state,
      error));
  CHECK(error.empty());
  CHECK(static_cast<bool>(state.inferCallExprBaseKind));

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo fileHandle;
  fileHandle.isFileHandle = true;
  locals.emplace("fh", fileHandle);

  primec::Expr fieldReceiver;
  fieldReceiver.kind = primec::Expr::Kind::Name;
  fieldReceiver.name = "receiver";
  primec::Expr fieldAccess;
  fieldAccess.kind = primec::Expr::Kind::Call;
  fieldAccess.isFieldAccess = true;
  fieldAccess.name = "value";
  fieldAccess.args.push_back(fieldReceiver);
  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(state.inferCallExprBaseKind(fieldAccess, locals, kindOut));
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Float32);

  primec::Expr storageExpr;
  storageExpr.kind = primec::Expr::Kind::Name;
  storageExpr.name = "slot";
  primec::Expr takeExpr;
  takeExpr.kind = primec::Expr::Kind::Call;
  takeExpr.name = "take";
  takeExpr.args.push_back(storageExpr);
  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(state.inferCallExprBaseKind(takeExpr, locals, kindOut));
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Int64);

  primec::Expr fileCall;
  fileCall.kind = primec::Expr::Kind::Call;
  fileCall.name = "File";
  fileCall.namespacePrefix = "/std/file";
  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(state.inferCallExprBaseKind(fileCall, locals, kindOut));
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Int64);
}

TEST_SUITE_END();
