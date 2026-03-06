
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

  const ir_lowerer::LowerInferenceReturnInfoSetupInput returnInfoSetupInput = {
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
  };
  const ir_lowerer::LowerInferenceGetReturnInfoStepInput getReturnInfoStepInput = {
      .defMap = &defMap,
      .returnInfoCache = &returnInfoCache,
      .returnInferenceStack = &returnInferenceStack,
      .returnInfoSetupInput = &returnInfoSetupInput,
  };
  if (!ir_lowerer::runLowerInferenceGetReturnInfoCallbackSetup(
          {
              .defMap = getReturnInfoStepInput.defMap,
              .returnInfoCache = getReturnInfoStepInput.returnInfoCache,
              .returnInferenceStack = getReturnInfoStepInput.returnInferenceStack,
              .returnInfoSetupInput = getReturnInfoStepInput.returnInfoSetupInput,
              .error = &error,
          },
          getReturnInfo,
          error)) {
    return false;
  }
