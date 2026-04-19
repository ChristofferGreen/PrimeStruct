
  ir_lowerer::LowerInferenceSetupBootstrapState inferenceSetupBootstrap;
  if (!ir_lowerer::runLowerInferenceSetup(
          {
              .program = &program,
              .defMap = &defMap,
              .importAliases = &importAliases,
              .structNames = &structNames,
              .semanticProgram = callResolutionAdapters.semanticProgram,
              .semanticIndex = &callResolutionAdapters.semanticProductTargets.semanticIndex,
              .isArrayCountCall = isArrayCountCall,
              .isStringCountCall = isStringCountCall,
              .isVectorCapacityCall = isVectorCapacityCall,
              .isEntryArgsName = isEntryArgsName,
              .resolveExprPath = resolveExprPath,
              .getBuiltinOperatorName = getBuiltinOperatorName,
              .resolveStructArrayInfoFromPath = resolveStructArrayInfoFromPath,
              .inferStructExprPath = inferStructExprPath,
              .resolveStructFieldSlot = resolveStructFieldSlot,
              .resolveUninitializedStorage = resolveUninitializedStorage,
              .hasMathImport = hasMathImport,
              .combineNumericKinds = combineNumericKinds,
              .getMathConstantName = getMathConstantName,
              .resolveStructTypeName = resolveStructTypeName,
              .isBindingMutable = isBindingMutable,
              .bindingKind = bindingKind,
              .hasExplicitBindingTypeTransform = hasExplicitBindingTypeTransform,
              .bindingValueKind = bindingValueKind,
              .isFileErrorBinding = isFileErrorBinding,
              .setReferenceArrayInfo = setReferenceArrayInfo,
              .applyStructArrayInfo = applyStructArrayInfo,
              .applyStructValueInfo = applyStructValueInfo,
              .isStringBinding = isStringBinding,
              .lowerMatchToIf = lowerMatchToIf,
          },
          inferenceSetupBootstrap,
          error)) {
    return false;
  }

  auto &getReturnInfo = inferenceSetupBootstrap.getReturnInfo;
  auto &inferExprKind = inferenceSetupBootstrap.inferExprKind;
  auto &inferArrayElementKind = inferenceSetupBootstrap.inferArrayElementKind;
  auto &resolveMethodCallDefinition = inferenceSetupBootstrap.resolveMethodCallDefinition;
