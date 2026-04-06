TEST_CASE("ir lowerer inference get-return-info step reports missing definitions") {
  std::unordered_map<std::string, const primec::Definition *> defMap;
  std::unordered_map<std::string, primec::ir_lowerer::ReturnInfo> returnInfoCache;
  std::unordered_set<std::string> returnInferenceStack;
  const primec::ir_lowerer::LowerInferenceReturnInfoSetupInput returnInfoSetupInput = {
      .resolveStructTypeName = [](const std::string &, const std::string &, std::string &) { return false; },
      .resolveStructArrayInfoFromPath = [](const std::string &, primec::ir_lowerer::StructArrayTypeInfo &) { return false; },
      .isBindingMutable = [](const primec::Expr &) { return false; },
      .bindingKind = [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::Kind::Value; },
      .hasExplicitBindingTypeTransform = [](const primec::Expr &) { return true; },
      .bindingValueKind = [](const primec::Expr &, primec::ir_lowerer::LocalInfo::Kind) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Int32;
      },
      .inferExprKind = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      .isFileErrorBinding = [](const primec::Expr &) { return false; },
      .setReferenceArrayInfo = [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      .applyStructArrayInfo = [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      .applyStructValueInfo = [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      .inferStructExprPath = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string{}; },
      .isStringBinding = [](const primec::Expr &) { return false; },
      .inferArrayElementKind = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      .lowerMatchToIf = [](const primec::Expr &, primec::Expr &, std::string &) { return true; },
  };
  const primec::ir_lowerer::LowerInferenceGetReturnInfoStepInput input = {
      .defMap = &defMap,
      .returnInfoCache = &returnInfoCache,
      .returnInferenceStack = &returnInferenceStack,
      .returnInfoSetupInput = &returnInfoSetupInput,
  };

  primec::ir_lowerer::ReturnInfo outInfo;
  std::string error;
  CHECK_FALSE(primec::ir_lowerer::runLowerInferenceGetReturnInfoStep(input, "/missing", outInfo, error));
  CHECK(error == "native backend cannot resolve definition: /missing");
}

TEST_CASE("ir lowerer inference get-return-info step rejects recursive lookup") {
  primec::Definition definition;
  definition.fullPath = "/callee";
  std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/callee", &definition},
  };
  std::unordered_map<std::string, primec::ir_lowerer::ReturnInfo> returnInfoCache;
  std::unordered_set<std::string> returnInferenceStack = {
      "/callee",
  };
  const primec::ir_lowerer::LowerInferenceReturnInfoSetupInput returnInfoSetupInput = {
      .resolveStructTypeName = [](const std::string &, const std::string &, std::string &) { return false; },
      .resolveStructArrayInfoFromPath = [](const std::string &, primec::ir_lowerer::StructArrayTypeInfo &) { return false; },
      .isBindingMutable = [](const primec::Expr &) { return false; },
      .bindingKind = [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::Kind::Value; },
      .hasExplicitBindingTypeTransform = [](const primec::Expr &) { return true; },
      .bindingValueKind = [](const primec::Expr &, primec::ir_lowerer::LocalInfo::Kind) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Int32;
      },
      .inferExprKind = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      .isFileErrorBinding = [](const primec::Expr &) { return false; },
      .setReferenceArrayInfo = [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      .applyStructArrayInfo = [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      .applyStructValueInfo = [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      .inferStructExprPath = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string{}; },
      .isStringBinding = [](const primec::Expr &) { return false; },
      .inferArrayElementKind = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      .lowerMatchToIf = [](const primec::Expr &, primec::Expr &, std::string &) { return true; },
  };
  const primec::ir_lowerer::LowerInferenceGetReturnInfoStepInput input = {
      .defMap = &defMap,
      .returnInfoCache = &returnInfoCache,
      .returnInferenceStack = &returnInferenceStack,
      .returnInfoSetupInput = &returnInfoSetupInput,
  };

  primec::ir_lowerer::ReturnInfo outInfo;
  std::string error;
  CHECK_FALSE(primec::ir_lowerer::runLowerInferenceGetReturnInfoStep(input, "/callee", outInfo, error));
  CHECK(error == "native backend return type inference requires explicit annotation on /callee");
}

TEST_CASE("ir lowerer inference get-return-info step validates dependencies") {
  primec::ir_lowerer::LowerInferenceGetReturnInfoStepInput input;
  primec::ir_lowerer::ReturnInfo outInfo;
  std::string error;
  CHECK_FALSE(primec::ir_lowerer::runLowerInferenceGetReturnInfoStep(input, "/callee", outInfo, error));
  CHECK(error == "native backend missing inference get-return-info step dependency: defMap");
}

TEST_CASE("ir lowerer inference get-return-info callback setup wires callback") {
  primec::Definition definition;
  definition.fullPath = "/callee";
  primec::Transform returnTransform;
  returnTransform.name = "return";
  returnTransform.templateArgs = {"i64"};
  definition.transforms.push_back(returnTransform);

  std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/callee", &definition},
  };
  std::unordered_map<std::string, primec::ir_lowerer::ReturnInfo> returnInfoCache;
  std::unordered_set<std::string> returnInferenceStack;
  const primec::ir_lowerer::LowerInferenceReturnInfoSetupInput returnInfoSetupInput = {
      .resolveStructTypeName = [](const std::string &, const std::string &, std::string &) { return false; },
      .resolveStructArrayInfoFromPath = [](const std::string &, primec::ir_lowerer::StructArrayTypeInfo &) { return false; },
      .isBindingMutable = [](const primec::Expr &) { return false; },
      .bindingKind = [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::Kind::Value; },
      .hasExplicitBindingTypeTransform = [](const primec::Expr &) { return true; },
      .bindingValueKind = [](const primec::Expr &, primec::ir_lowerer::LocalInfo::Kind) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Int32;
      },
      .inferExprKind = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      .isFileErrorBinding = [](const primec::Expr &) { return false; },
      .setReferenceArrayInfo = [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      .applyStructArrayInfo = [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      .applyStructValueInfo = [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      .inferStructExprPath = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string{}; },
      .isStringBinding = [](const primec::Expr &) { return false; },
      .inferArrayElementKind = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      .lowerMatchToIf = [](const primec::Expr &, primec::Expr &, std::string &) { return true; },
  };

  std::function<bool(const std::string &, primec::ir_lowerer::ReturnInfo &)> getReturnInfo;
  std::string inferenceError;
  std::string error;
  CHECK(primec::ir_lowerer::runLowerInferenceGetReturnInfoCallbackSetup(
      {
          .defMap = &defMap,
          .returnInfoCache = &returnInfoCache,
          .returnInferenceStack = &returnInferenceStack,
          .returnInfoSetupInput = &returnInfoSetupInput,
          .error = &inferenceError,
      },
      getReturnInfo,
      error));
  CHECK(error.empty());
  CHECK(static_cast<bool>(getReturnInfo));

  primec::ir_lowerer::ReturnInfo outInfo;
  CHECK(getReturnInfo("/callee", outInfo));
  CHECK(outInfo.kind == primec::ir_lowerer::LocalInfo::ValueKind::Int64);
  CHECK(returnInfoCache.count("/callee") == 1);
}

TEST_CASE("ir lowerer inference get-return-info callback setup validates dependencies") {
  std::function<bool(const std::string &, primec::ir_lowerer::ReturnInfo &)> getReturnInfo;
  std::string error;
  CHECK_FALSE(primec::ir_lowerer::runLowerInferenceGetReturnInfoCallbackSetup(
      {},
      getReturnInfo,
      error));
  CHECK(error == "native backend missing inference get-return-info callback setup dependency: defMap");
  CHECK_FALSE(static_cast<bool>(getReturnInfo));
}

TEST_CASE("ir lowerer inference get-return-info setup wires callback") {
  primec::Program program;
  program.definitions.emplace_back();
  primec::Definition &definition = program.definitions.back();
  definition.fullPath = "/callee";
  primec::Transform returnTransform;
  returnTransform.name = "return";
  returnTransform.templateArgs = {"i64"};
  definition.transforms.push_back(returnTransform);

  std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/callee", &definition},
  };
  std::unordered_map<std::string, primec::ir_lowerer::ReturnInfo> returnInfoCache;
  std::unordered_set<std::string> returnInferenceStack;

  std::function<bool(const std::string &, primec::ir_lowerer::ReturnInfo &)> getReturnInfo;
  std::string inferenceError;
  std::string error;
  CHECK(primec::ir_lowerer::runLowerInferenceGetReturnInfoSetup(
      {
          .program = &program,
          .defMap = &defMap,
          .returnInfoCache = &returnInfoCache,
          .returnInferenceStack = &returnInferenceStack,
          .resolveStructTypeName = [](const std::string &, const std::string &, std::string &) { return false; },
          .resolveStructArrayInfoFromPath =
              [](const std::string &, primec::ir_lowerer::StructArrayTypeInfo &) { return false; },
          .isBindingMutable = [](const primec::Expr &) { return false; },
          .bindingKind = [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::Kind::Value; },
          .hasExplicitBindingTypeTransform = [](const primec::Expr &) { return true; },
          .bindingValueKind = [](const primec::Expr &, primec::ir_lowerer::LocalInfo::Kind) {
            return primec::ir_lowerer::LocalInfo::ValueKind::Int32;
          },
          .inferExprKind = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
            return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
          },
          .isFileErrorBinding = [](const primec::Expr &) { return false; },
          .setReferenceArrayInfo = [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
          .applyStructArrayInfo = [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
          .applyStructValueInfo = [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
          .inferStructExprPath = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
            return std::string{};
          },
          .isStringBinding = [](const primec::Expr &) { return false; },
          .inferArrayElementKind = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
            return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
          },
          .lowerMatchToIf = [](const primec::Expr &, primec::Expr &, std::string &) { return true; },
          .error = &inferenceError,
      },
      getReturnInfo,
      error));
  CHECK(error.empty());
  CHECK(static_cast<bool>(getReturnInfo));

  primec::ir_lowerer::ReturnInfo outInfo;
  CHECK(getReturnInfo("/callee", outInfo));
  CHECK(outInfo.kind == primec::ir_lowerer::LocalInfo::ValueKind::Int64);
  CHECK(returnInfoCache.count("/callee") == 1);
}

TEST_CASE("ir lowerer inference get-return-info setup validates dependencies") {
  primec::Program program;
  std::function<bool(const std::string &, primec::ir_lowerer::ReturnInfo &)> getReturnInfo;
  std::string error;
  CHECK_FALSE(primec::ir_lowerer::runLowerInferenceGetReturnInfoSetup(
      {
          .program = &program,
      },
      getReturnInfo,
      error));
  CHECK(error == "native backend missing inference get-return-info setup dependency: defMap");
  CHECK_FALSE(static_cast<bool>(getReturnInfo));
}

TEST_CASE("ir lowerer inference get-return-info step uses semantic-product return metadata") {
  primec::Definition definition;
  definition.fullPath = "/pkg/current";
  definition.namespacePrefix = "/pkg";
  definition.semanticNodeId = 91;

  std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/pkg/current", &definition},
  };
  std::unordered_map<std::string, primec::ir_lowerer::ReturnInfo> returnInfoCache;
  std::unordered_set<std::string> returnInferenceStack;
  const primec::ir_lowerer::LowerInferenceReturnInfoSetupInput returnInfoSetupInput = {
      .resolveStructTypeName = [](const std::string &typeName, const std::string &, std::string &structPathOut) {
        if (typeName == "Pair") {
          structPathOut = "/pkg/Pair";
          return true;
        }
        return false;
      },
      .resolveStructArrayInfoFromPath = [](const std::string &, primec::ir_lowerer::StructArrayTypeInfo &) {
        return false;
      },
      .isBindingMutable = [](const primec::Expr &) { return false; },
      .bindingKind = [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::Kind::Value; },
      .hasExplicitBindingTypeTransform = [](const primec::Expr &) { return true; },
      .bindingValueKind = [](const primec::Expr &, primec::ir_lowerer::LocalInfo::Kind) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Int32;
      },
      .inferExprKind = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      .isFileErrorBinding = [](const primec::Expr &) { return false; },
      .setReferenceArrayInfo = [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      .applyStructArrayInfo = [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      .applyStructValueInfo = [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      .inferStructExprPath = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string{}; },
      .isStringBinding = [](const primec::Expr &) { return false; },
      .inferArrayElementKind = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      .lowerMatchToIf = [](const primec::Expr &, primec::Expr &, std::string &) { return true; },
  };

  primec::SemanticProgram semanticProgram;
  semanticProgram.callableSummaries.push_back(primec::SemanticProgramCallableSummary{
      .fullPath = "/pkg/current",
      .isExecution = false,
      .returnKind = "int64",
      .isCompute = false,
      .isUnsafe = false,
      .activeEffects = {},
      .activeCapabilities = {},
      .hasResultType = true,
      .resultTypeHasValue = true,
      .resultValueType = "Pair",
      .resultErrorType = "MyError",
      .hasOnError = false,
      .onErrorHandlerPath = "",
      .onErrorErrorType = "",
      .onErrorBoundArgCount = 0,
      .semanticNodeId = 91,
  });
  semanticProgram.returnFacts.push_back(primec::SemanticProgramReturnFact{
      .definitionPath = "/pkg/legacy",
      .returnKind = "int64",
      .structPath = "/pkg/Pair",
      .bindingTypeText = "Result<Pair, MyError>",
      .isMutable = false,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 91,
  });
  const auto semanticProductTargets = primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);

  const primec::ir_lowerer::LowerInferenceGetReturnInfoStepInput input = {
      .defMap = &defMap,
      .returnInfoCache = &returnInfoCache,
      .returnInferenceStack = &returnInferenceStack,
      .returnInfoSetupInput = &returnInfoSetupInput,
      .semanticProductTargets = &semanticProductTargets,
  };

  primec::ir_lowerer::ReturnInfo outInfo;
  std::string error;
  CHECK(primec::ir_lowerer::runLowerInferenceGetReturnInfoStep(input, "/pkg/current", outInfo, error));
  CHECK(error.empty());
  CHECK(outInfo.isResult);
  CHECK(outInfo.resultHasValue);
  CHECK(outInfo.resultValueStructType == "/pkg/Pair");
  CHECK(outInfo.resultErrorType == "MyError");
  CHECK(returnInfoCache.count("/pkg/current") == 1);
}

TEST_CASE("ir lowerer inference get-return-info setup precomputes semantic-product return metadata") {
  primec::Program program;
  program.definitions.emplace_back();
  primec::Definition &definition = program.definitions.back();
  definition.fullPath = "/pkg/current";
  definition.namespacePrefix = "/pkg";
  definition.semanticNodeId = 97;

  std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/pkg/current", &definition},
  };
  std::unordered_map<std::string, primec::ir_lowerer::ReturnInfo> returnInfoCache;
  std::unordered_set<std::string> returnInferenceStack;

  primec::SemanticProgram semanticProgram;
  semanticProgram.callableSummaries.push_back(primec::SemanticProgramCallableSummary{
      .fullPath = "/pkg/current",
      .isExecution = false,
      .returnKind = "array",
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
      .semanticNodeId = 97,
  });
  semanticProgram.returnFacts.push_back(primec::SemanticProgramReturnFact{
      .definitionPath = "/pkg/current",
      .returnKind = "array",
      .structPath = "/vector",
      .bindingTypeText = "vector<Pair>",
      .isMutable = false,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 97,
  });
  const auto semanticProductTargets = primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);

  std::function<bool(const std::string &, primec::ir_lowerer::ReturnInfo &)> getReturnInfo;
  std::string inferenceError;
  std::string error;
  CHECK(primec::ir_lowerer::runLowerInferenceGetReturnInfoSetup(
      {
          .program = &program,
          .defMap = &defMap,
          .returnInfoCache = &returnInfoCache,
          .returnInferenceStack = &returnInferenceStack,
          .semanticProductTargets = &semanticProductTargets,
          .resolveStructTypeName = [](const std::string &typeName, const std::string &, std::string &structPathOut) {
            if (typeName == "Pair") {
              structPathOut = "/pkg/Pair";
              return true;
            }
            return false;
          },
          .resolveStructArrayInfoFromPath =
              [](const std::string &, primec::ir_lowerer::StructArrayTypeInfo &) { return false; },
          .isBindingMutable = [](const primec::Expr &) { return false; },
          .bindingKind = [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::Kind::Value; },
          .hasExplicitBindingTypeTransform = [](const primec::Expr &) { return true; },
          .bindingValueKind = [](const primec::Expr &, primec::ir_lowerer::LocalInfo::Kind) {
            return primec::ir_lowerer::LocalInfo::ValueKind::Int32;
          },
          .inferExprKind = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
            return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
          },
          .isFileErrorBinding = [](const primec::Expr &) { return false; },
          .setReferenceArrayInfo = [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
          .applyStructArrayInfo = [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
          .applyStructValueInfo = [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
          .inferStructExprPath = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
            return std::string{};
          },
          .isStringBinding = [](const primec::Expr &) { return false; },
          .inferArrayElementKind = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
            return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
          },
          .lowerMatchToIf = [](const primec::Expr &, primec::Expr &, std::string &) { return true; },
          .error = &inferenceError,
      },
      getReturnInfo,
      error));
  CHECK(error.empty());
  CHECK(static_cast<bool>(getReturnInfo));
  CHECK(returnInfoCache.count("/pkg/current") == 1);

  defMap.clear();
  primec::ir_lowerer::ReturnInfo outInfo;
  CHECK(getReturnInfo("/pkg/current", outInfo));
  CHECK(outInfo.returnsArray);
  CHECK(outInfo.kind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
}

TEST_CASE("ir lowerer inference preserves map reference parameter info for canonical count auto wrappers") {
  primec::Program program;
  program.definitions.reserve(2);
  program.definitions.emplace_back();
  program.definitions.emplace_back();
  primec::Definition &canonicalCountDef = program.definitions[0];
  primec::Definition &projectDef = program.definitions[1];
  canonicalCountDef.fullPath = "/std/collections/map/count";
  primec::Transform countReturnTransform;
  countReturnTransform.name = "return";
  countReturnTransform.templateArgs = {"int"};
  canonicalCountDef.transforms.push_back(countReturnTransform);

  projectDef.fullPath = "/project";
  primec::Transform projectReturnTransform;
  projectReturnTransform.name = "return";
  projectReturnTransform.templateArgs = {"auto"};
  projectDef.transforms.push_back(projectReturnTransform);

  primec::Expr refParam;
  refParam.isBinding = true;
  refParam.name = "ref";
  primec::Transform refTransform;
  refTransform.name = "Reference";
  refTransform.templateArgs = {"std/collections/map<i32, i32>"};
  refParam.transforms.push_back(refTransform);
  projectDef.parameters.push_back(refParam);

  primec::Expr countCall;
  countCall.kind = primec::Expr::Kind::Call;
  countCall.name = "/std/collections/map/count";
  primec::Expr refName;
  refName.kind = primec::Expr::Kind::Name;
  refName.name = "ref";
  primec::Expr markerArg;
  markerArg.kind = primec::Expr::Kind::BoolLiteral;
  markerArg.boolValue = true;
  countCall.args = {refName, markerArg};
  projectDef.returnExpr = countCall;

  std::unordered_map<std::string, const primec::Definition *> defMap = {
      {canonicalCountDef.fullPath, &canonicalCountDef},
      {projectDef.fullPath, &projectDef},
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
          .resolveExprPath = [](const primec::Expr &expr) {
            if (expr.kind == primec::Expr::Kind::Call && !expr.name.empty() && expr.name.front() != '/') {
              return std::string("/") + expr.name;
            }
            return expr.name;
          },
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
              [](primec::ir_lowerer::LocalInfo::ValueKind left, primec::ir_lowerer::LocalInfo::ValueKind right) {
                return (left == right) ? left : primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
              },
          .getMathConstantName = [](const std::string &, std::string &) { return false; },
          .resolveStructTypeName = [](const std::string &, const std::string &, std::string &) { return false; },
          .isBindingMutable = [](const primec::Expr &) { return false; },
          .bindingKind = [](const primec::Expr &expr) { return primec::ir_lowerer::bindingKindFromTransforms(expr); },
          .hasExplicitBindingTypeTransform =
              [](const primec::Expr &expr) { return primec::ir_lowerer::hasExplicitBindingTypeTransform(expr); },
          .bindingValueKind = [](const primec::Expr &expr, primec::ir_lowerer::LocalInfo::Kind kind) {
            return primec::ir_lowerer::bindingValueKindFromTransforms(expr, kind);
          },
          .isFileErrorBinding = [](const primec::Expr &expr) { return primec::ir_lowerer::isFileErrorBindingType(expr); },
          .setReferenceArrayInfo = [](const primec::Expr &expr, primec::ir_lowerer::LocalInfo &info) {
            primec::ir_lowerer::setReferenceArrayInfoFromTransforms(expr, info);
          },
          .applyStructArrayInfo = [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
          .applyStructValueInfo = [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
          .isStringBinding = [](const primec::Expr &expr) { return primec::ir_lowerer::isStringBindingType(expr); },
          .lowerMatchToIf = [](const primec::Expr &expr, primec::Expr &expandedExpr, std::string &) {
            expandedExpr = expr;
            return true;
          },
      },
      state,
      error));
  CHECK(error.empty());

  primec::ir_lowerer::ReturnInfo returnInfo;
  CHECK(state.getReturnInfo("/project", returnInfo));
  CHECK_FALSE(returnInfo.returnsVoid);
  CHECK_FALSE(returnInfo.returnsArray);
  CHECK(returnInfo.kind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
}

TEST_CASE("ir lowerer inference expr-kind dispatch setup wires callback") {
  primec::ir_lowerer::LowerInferenceSetupBootstrapState state;
  state.inferLiteralOrNameExprKind = [](const primec::Expr &expr,
                                        const primec::ir_lowerer::LocalMap &,
                                        primec::ir_lowerer::LocalInfo::ValueKind &kindOut) {
    kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
    if (expr.kind == primec::Expr::Kind::BoolLiteral) {
      kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Bool;
      return true;
    }
    if (expr.kind == primec::Expr::Kind::StringLiteral) {
      kindOut = primec::ir_lowerer::LocalInfo::ValueKind::String;
      return true;
    }
    return false;
  };
  state.inferCallExprBaseKind = [](const primec::Expr &expr,
                                   const primec::ir_lowerer::LocalMap &,
                                   primec::ir_lowerer::LocalInfo::ValueKind &kindOut) {
    kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
    if (expr.name == "base") {
      kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
      return true;
    }
    return false;
  };
  state.inferCallExprDirectReturnKind = [](const primec::Expr &expr,
                                           const primec::ir_lowerer::LocalMap &,
                                           primec::ir_lowerer::LocalInfo::ValueKind &kindOut) {
    kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
    if (expr.name == "direct") {
      kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
      return primec::ir_lowerer::CallExpressionReturnKindResolution::Resolved;
    }
    if (expr.name == "unsupported") {
      return primec::ir_lowerer::CallExpressionReturnKindResolution::MatchedButUnsupported;
    }
    return primec::ir_lowerer::CallExpressionReturnKindResolution::NotResolved;
  };
  state.inferCallExprCountAccessGpuFallbackKind = [](const primec::Expr &expr,
                                                     const primec::ir_lowerer::LocalMap &,
                                                     primec::ir_lowerer::LocalInfo::ValueKind &kindOut) {
    kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
    if (expr.name == "fallback") {
      kindOut = primec::ir_lowerer::LocalInfo::ValueKind::UInt64;
      return true;
    }
    return false;
  };
  state.inferCallExprOperatorFallbackKind = [](const primec::Expr &expr,
                                               const primec::ir_lowerer::LocalMap &,
                                               primec::ir_lowerer::LocalInfo::ValueKind &kindOut) {
    kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
    if (expr.name == "operator") {
      kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Float64;
      return true;
    }
    return false;
  };
  state.inferCallExprControlFlowFallbackKind = [](const primec::Expr &expr,
                                                  const primec::ir_lowerer::LocalMap &,
                                                  std::string &,
                                                  primec::ir_lowerer::LocalInfo::ValueKind &kindOut) {
    kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
    if (expr.name == "flow") {
      kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Float32;
      return true;
    }
    return false;
  };
  state.inferCallExprPointerFallbackKind = [](const primec::Expr &expr,
                                              const primec::ir_lowerer::LocalMap &,
                                              primec::ir_lowerer::LocalInfo::ValueKind &kindOut) {
    kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
    if (expr.name == "pointer") {
      kindOut = primec::ir_lowerer::LocalInfo::ValueKind::String;
      return true;
    }
    return false;
  };

  primec::Definition resultDefinition;
  resultDefinition.fullPath = "/statusResult";
  const std::unordered_map<std::string, const primec::Definition *> defMap{
      {"/statusResult", &resultDefinition},
  };

  state.getReturnInfo = [](const std::string &path, primec::ir_lowerer::ReturnInfo &out) {
    if (path != "/statusResult") {
      return false;
    }
    out.kind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
    out.isResult = true;
    out.resultHasValue = true;
    out.resultValueKind = primec::ir_lowerer::LocalInfo::ValueKind::String;
    out.resultErrorType = "ContainerError";
    return true;
  };

  std::string inferenceError;
  std::string error;
  CHECK(primec::ir_lowerer::runLowerInferenceExprKindDispatchSetup(
      {
          .defMap = &defMap,
          .resolveExprPath = [](const primec::Expr &expr) { return "/" + expr.name; },
          .error = &inferenceError,
      },
      state,
      error));
  CHECK(error.empty());
  CHECK(static_cast<bool>(state.inferExprKind));

  primec::Expr boolExpr;
  boolExpr.kind = primec::Expr::Kind::BoolLiteral;
  CHECK(state.inferExprKind(boolExpr, primec::ir_lowerer::LocalMap{}) == primec::ir_lowerer::LocalInfo::ValueKind::Bool);

  primec::Expr directExpr;
  directExpr.kind = primec::Expr::Kind::Call;
  directExpr.name = "direct";
  CHECK(state.inferExprKind(directExpr, primec::ir_lowerer::LocalMap{}) == primec::ir_lowerer::LocalInfo::ValueKind::Int64);

  primec::Expr unsupportedExpr;
  unsupportedExpr.kind = primec::Expr::Kind::Call;
  unsupportedExpr.name = "unsupported";
  CHECK(state.inferExprKind(unsupportedExpr, primec::ir_lowerer::LocalMap{}) ==
        primec::ir_lowerer::LocalInfo::ValueKind::Unknown);

  primec::Expr fallbackExpr;
  fallbackExpr.kind = primec::Expr::Kind::Call;
  fallbackExpr.name = "fallback";
  CHECK(state.inferExprKind(fallbackExpr, primec::ir_lowerer::LocalMap{}) == primec::ir_lowerer::LocalInfo::ValueKind::UInt64);

  primec::Expr pointerExpr;
  pointerExpr.kind = primec::Expr::Kind::Call;
  pointerExpr.name = "pointer";
  CHECK(state.inferExprKind(pointerExpr, primec::ir_lowerer::LocalMap{}) == primec::ir_lowerer::LocalInfo::ValueKind::String);

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo resultLocal;
  resultLocal.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  resultLocal.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  resultLocal.isResult = true;
  resultLocal.resultHasValue = true;
  resultLocal.resultValueKind = primec::ir_lowerer::LocalInfo::ValueKind::String;
  resultLocal.resultErrorType = "ContainerError";
  locals.emplace("status", resultLocal);

  primec::Expr localResultExpr;
  localResultExpr.kind = primec::Expr::Kind::Name;
  localResultExpr.name = "status";

  primec::Expr tryLocalExpr;
  tryLocalExpr.kind = primec::Expr::Kind::Call;
  tryLocalExpr.name = "try";
  tryLocalExpr.args.push_back(localResultExpr);
  CHECK(state.inferExprKind(tryLocalExpr, locals) == primec::ir_lowerer::LocalInfo::ValueKind::String);

  primec::Expr resultCallExpr;
  resultCallExpr.kind = primec::Expr::Kind::Call;
  resultCallExpr.name = "statusResult";

  primec::Expr tryCallExpr;
  tryCallExpr.kind = primec::Expr::Kind::Call;
  tryCallExpr.name = "try";
  tryCallExpr.args.push_back(resultCallExpr);
  CHECK(state.inferExprKind(tryCallExpr, locals) == primec::ir_lowerer::LocalInfo::ValueKind::String);
}

TEST_CASE("ir lowerer inference expr-kind dispatch infers try from indexed pointer Result args packs") {
  primec::ir_lowerer::LowerInferenceSetupBootstrapState state;
  state.inferLiteralOrNameExprKind = [](const primec::Expr &expr,
                                        const primec::ir_lowerer::LocalMap &,
                                        primec::ir_lowerer::LocalInfo::ValueKind &kindOut) {
    kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
    if (expr.kind == primec::Expr::Kind::Literal) {
      kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
      return true;
    }
    return false;
  };
  state.inferCallExprBaseKind = [](const primec::Expr &,
                                   const primec::ir_lowerer::LocalMap &,
                                   primec::ir_lowerer::LocalInfo::ValueKind &kindOut) {
    kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
    return false;
  };
  state.inferCallExprDirectReturnKind = [](const primec::Expr &,
                                           const primec::ir_lowerer::LocalMap &,
                                           primec::ir_lowerer::LocalInfo::ValueKind &kindOut) {
    kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
    return primec::ir_lowerer::CallExpressionReturnKindResolution::NotResolved;
  };
  state.inferCallExprCountAccessGpuFallbackKind = [](const primec::Expr &,
                                                     const primec::ir_lowerer::LocalMap &,
                                                     primec::ir_lowerer::LocalInfo::ValueKind &kindOut) {
    kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
    return false;
  };
  state.resolveMethodCallDefinition =
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
    return nullptr;
  };
  state.inferCallExprOperatorFallbackKind = [](const primec::Expr &,
                                               const primec::ir_lowerer::LocalMap &,
                                               primec::ir_lowerer::LocalInfo::ValueKind &kindOut) {
    kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
    return false;
  };
  state.inferCallExprControlFlowFallbackKind = [](const primec::Expr &,
                                                  const primec::ir_lowerer::LocalMap &,
                                                  std::string &,
                                                  primec::ir_lowerer::LocalInfo::ValueKind &kindOut) {
    kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
    return false;
  };
  state.inferCallExprPointerFallbackKind = [](const primec::Expr &,
                                              const primec::ir_lowerer::LocalMap &,
                                              primec::ir_lowerer::LocalInfo::ValueKind &kindOut) {
    kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
    return false;
  };

  std::unordered_map<std::string, const primec::Definition *> defMap;
  std::string inferenceError;
  std::string error;
  CHECK(primec::ir_lowerer::runLowerInferenceExprKindDispatchSetup(
      {
          .defMap = &defMap,
          .resolveExprPath = [](const primec::Expr &expr) { return "/" + expr.name; },
          .error = &inferenceError,
      },
      state,
      error));
  CHECK(error.empty());
  CHECK(static_cast<bool>(state.inferExprKind));

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo valuesInfo;
  valuesInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  valuesInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  valuesInfo.isArgsPack = true;
  valuesInfo.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Pointer;
  valuesInfo.isResult = true;
  valuesInfo.resultHasValue = true;
  valuesInfo.resultValueKind = primec::ir_lowerer::LocalInfo::ValueKind::String;
  valuesInfo.resultErrorType = "ParseError";
  locals.emplace("values", valuesInfo);

  primec::Expr valuesExpr;
  valuesExpr.kind = primec::Expr::Kind::Name;
  valuesExpr.name = "values";

  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Literal;
  indexExpr.literalValue = 0;

  primec::Expr atExpr;
  atExpr.kind = primec::Expr::Kind::Call;
  atExpr.name = "at";
  atExpr.args.push_back(valuesExpr);
  atExpr.args.push_back(indexExpr);

  primec::Expr dereferenceExpr;
  dereferenceExpr.kind = primec::Expr::Kind::Call;
  dereferenceExpr.name = "dereference";
  dereferenceExpr.args.push_back(atExpr);

  primec::Expr tryExpr;
  tryExpr.kind = primec::Expr::Kind::Call;
  tryExpr.name = "try";
  tryExpr.args.push_back(dereferenceExpr);

  CHECK(state.inferExprKind(tryExpr, locals) == primec::ir_lowerer::LocalInfo::ValueKind::String);
}
