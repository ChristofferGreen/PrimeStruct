
  ir_lowerer::LowerInferenceSetupBootstrapState inferenceSetupBootstrap;
  if (!ir_lowerer::runLowerInferenceSetupBootstrap(
          {
              .defMap = &defMap,
              .importAliases = &importAliases,
              .structNames = &structNames,
              .isArrayCountCall = isArrayCountCall,
              .isVectorCapacityCall = isVectorCapacityCall,
              .isEntryArgsName = isEntryArgsName,
              .resolveExprPath = resolveExprPath,
              .getBuiltinOperatorName = getBuiltinOperatorName,
          },
          inferenceSetupBootstrap,
          error)) {
    return false;
  }

  auto &returnInfoCache = inferenceSetupBootstrap.returnInfoCache;
  auto &returnInferenceStack = inferenceSetupBootstrap.returnInferenceStack;
  auto &getReturnInfo = inferenceSetupBootstrap.getReturnInfo;
  auto &inferExprKind = inferenceSetupBootstrap.inferExprKind;
  auto &inferArrayElementKind = inferenceSetupBootstrap.inferArrayElementKind;
  auto &resolveMethodCallDefinition = inferenceSetupBootstrap.resolveMethodCallDefinition;

  if (!ir_lowerer::runLowerInferenceArrayKindSetup(
          {
              .defMap = &defMap,
              .resolveExprPath = resolveExprPath,
              .resolveStructArrayInfoFromPath = resolveStructArrayInfoFromPath,
              .isArrayCountCall = isArrayCountCall,
              .isStringCountCall = isStringCountCall,
          },
          inferenceSetupBootstrap,
          error)) {
    return false;
  }
  if (!ir_lowerer::runLowerInferenceExprKindCallBaseSetup(
          {
              .inferStructExprPath = inferStructExprPath,
              .resolveStructFieldSlot = resolveStructFieldSlot,
              .resolveUninitializedStorage = resolveUninitializedStorage,
          },
          inferenceSetupBootstrap,
          error)) {
    return false;
  }
  if (!ir_lowerer::runLowerInferenceExprKindCallReturnSetup(
          {
              .defMap = &defMap,
              .resolveExprPath = resolveExprPath,
              .isArrayCountCall = isArrayCountCall,
              .isStringCountCall = isStringCountCall,
          },
          inferenceSetupBootstrap,
          error)) {
    return false;
  }
  if (!ir_lowerer::runLowerInferenceExprKindCallFallbackSetup(
          {
              .isArrayCountCall = isArrayCountCall,
              .isStringCountCall = isStringCountCall,
              .isVectorCapacityCall = isVectorCapacityCall,
              .isEntryArgsName = isEntryArgsName,
          },
          inferenceSetupBootstrap,
          error)) {
    return false;
  }
  if (!ir_lowerer::runLowerInferenceExprKindCallOperatorFallbackSetup(
          {
              .hasMathImport = hasMathImport,
              .combineNumericKinds = combineNumericKinds,
          },
          inferenceSetupBootstrap,
          error)) {
    return false;
  }
  if (!ir_lowerer::runLowerInferenceExprKindCallControlFlowFallbackSetup(
          {
              .defMap = &defMap,
              .resolveExprPath = resolveExprPath,
              .lowerMatchToIf = lowerMatchToIf,
              .combineNumericKinds = combineNumericKinds,
              .isBindingMutable = isBindingMutable,
              .bindingKind = bindingKind,
              .hasExplicitBindingTypeTransform = hasExplicitBindingTypeTransform,
              .bindingValueKind = bindingValueKind,
              .applyStructArrayInfo = applyStructArrayInfo,
              .applyStructValueInfo = applyStructValueInfo,
              .inferStructExprPath = inferStructExprPath,
          },
          inferenceSetupBootstrap,
          error)) {
    return false;
  }
  if (!ir_lowerer::runLowerInferenceExprKindCallPointerFallbackSetup(
          {},
          inferenceSetupBootstrap,
          error)) {
    return false;
  }
  if (!ir_lowerer::runLowerInferenceExprKindBaseSetup(
          {
              .getMathConstantName = getMathConstantName,
          },
          inferenceSetupBootstrap,
          error)) {
    return false;
  }

  if (!ir_lowerer::runLowerInferenceExprKindDispatchSetup(
          {
              .error = &error,
          },
          inferenceSetupBootstrap,
          error)) {
    return false;
  }

  getReturnInfo = [&](const std::string &path, ReturnInfo &outInfo) -> bool {
    auto cached = returnInfoCache.find(path);
    if (cached != returnInfoCache.end()) {
      outInfo = cached->second;
      return true;
    }
    auto defIt = defMap.find(path);
    if (defIt == defMap.end() || !defIt->second) {
      error = "native backend cannot resolve definition: " + path;
      return false;
    }
    if (!returnInferenceStack.insert(path).second) {
      error = "native backend return type inference requires explicit annotation on " + path;
      return false;
    }

    const Definition &def = *defIt->second;
    ReturnInfo info;
    if (!ir_lowerer::runLowerInferenceReturnInfoSetup(
            {
                .resolveStructTypeName = resolveStructTypeName,
                .resolveStructArrayInfoFromPath = resolveStructArrayInfoFromPath,
                .isBindingMutable = isBindingMutable,
                .bindingKind = bindingKind,
                .hasExplicitBindingTypeTransform = hasExplicitBindingTypeTransform,
                .bindingValueKind = bindingValueKind,
                .inferExprKind = inferExprKind,
                .isFileErrorBinding = isFileErrorBinding,
                .applyStructArrayInfo = applyStructArrayInfo,
                .applyStructValueInfo = applyStructValueInfo,
                .inferStructExprPath = inferStructExprPath,
                .isStringBinding = isStringBinding,
                .inferArrayElementKind = inferArrayElementKind,
                .lowerMatchToIf = lowerMatchToIf,
            },
            def,
            info,
            error)) {
      returnInferenceStack.erase(path);
      return false;
    }

    returnInferenceStack.erase(path);
    returnInfoCache.emplace(path, info);
    outInfo = info;
    return true;
  };
