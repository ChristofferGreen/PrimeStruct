  auto valueKindFromTypeName = ir_lowerer::makeValueKindFromTypeName();

  const bool hasMathImport = ir_lowerer::hasProgramMathImport(program.imports);

  bool returnsVoid = false;
  ResultReturnInfo entryResultInfo;
  bool entryHasResultInfo = false;
  EntryReturnConfig entryReturnConfig;
  if (!ir_lowerer::analyzeEntryReturnTransforms(*entryDef, entryPath, entryReturnConfig, error)) {
    return false;
  }
  returnsVoid = entryReturnConfig.returnsVoid;
  entryResultInfo = entryReturnConfig.resultInfo;
  entryHasResultInfo = entryReturnConfig.hasResultInfo;

  IrFunction function;
  function.name = entryPath;
  function.metadata.effectMask = entryEffectMask;
  function.metadata.capabilityMask = entryCapabilityMask;
  function.metadata.schedulingScope = IrSchedulingScope::Default;
  function.metadata.instrumentationFlags = 0;
  bool sawReturn = false;
  LocalMap locals;
  int32_t nextLocal = 0;
  int32_t onErrorTempCounter = 0;
  std::vector<std::string> stringTable;
  std::unordered_set<std::string> loweredCallTargets;
  std::vector<std::vector<int32_t>> fileScopeStack;
  std::optional<OnErrorHandler> currentOnError;
  std::optional<ResultReturnInfo> currentReturnResult;

  const auto stringLiteralHelpers = ir_lowerer::makeStringLiteralHelperContext(stringTable, error);
  auto internString = stringLiteralHelpers.internString;

  const auto runtimeErrorEmitters = ir_lowerer::makeRuntimeErrorEmitters(function, internString);
  auto emitArrayIndexOutOfBounds = runtimeErrorEmitters.emitArrayIndexOutOfBounds;
  auto emitStringIndexOutOfBounds = runtimeErrorEmitters.emitStringIndexOutOfBounds;
  auto emitMapKeyNotFound = runtimeErrorEmitters.emitMapKeyNotFound;
  auto emitVectorIndexOutOfBounds = runtimeErrorEmitters.emitVectorIndexOutOfBounds;
  auto emitVectorPopOnEmpty = runtimeErrorEmitters.emitVectorPopOnEmpty;
  auto emitVectorCapacityExceeded = runtimeErrorEmitters.emitVectorCapacityExceeded;
  auto emitVectorReserveNegative = runtimeErrorEmitters.emitVectorReserveNegative;
  auto emitVectorReserveExceeded = runtimeErrorEmitters.emitVectorReserveExceeded;
  auto emitLoopCountNegative = runtimeErrorEmitters.emitLoopCountNegative;
  auto emitPowNegativeExponent = runtimeErrorEmitters.emitPowNegativeExponent;
  auto emitFloatToIntNonFinite = runtimeErrorEmitters.emitFloatToIntNonFinite;

  bool hasEntryArgs = false;
  std::string entryArgsName;
  if (!ir_lowerer::resolveEntryArgsParameter(*entryDef, hasEntryArgs, entryArgsName, error)) {
    return false;
  }
  const auto countAccessClassifiers = ir_lowerer::makeCountAccessClassifiers(hasEntryArgs, entryArgsName);
  auto isEntryArgsName = countAccessClassifiers.isEntryArgsName;
  auto isArrayCountCall = countAccessClassifiers.isArrayCountCall;
  auto isVectorCapacityCall = countAccessClassifiers.isVectorCapacityCall;

  auto resolveStringTableTarget = stringLiteralHelpers.resolveStringTableTarget;

  auto isStringCountCall = countAccessClassifiers.isStringCountCall;

  auto resolveExprPath = ir_lowerer::makeResolveCallPathFromScope(defMap, importAliases);
  auto isTailCallCandidate = ir_lowerer::makeIsTailCallCandidate(defMap, resolveExprPath);
  const bool sawTailExecution =
      ir_lowerer::hasTailExecutionCandidate(entryDef->statements, returnsVoid, isTailCallCandidate);
  if (sawTailExecution) {
    function.metadata.instrumentationFlags |= InstrumentationTailExecution;
  }
  auto definitionExists = ir_lowerer::makeDefinitionExistsByPath(defMap);
  OnErrorByDefinition onErrorByDef;
  if (!ir_lowerer::buildOnErrorByDefinition(program, resolveExprPath, definitionExists, onErrorByDef, error)) {
    return false;
  }

  auto getMathBuiltinName = ir_lowerer::makeGetSetupMathBuiltinName(hasMathImport);
  auto getMathConstantName = ir_lowerer::makeGetSetupMathConstantName(hasMathImport);

  auto setReferenceArrayInfo = ir_lowerer::makeSetReferenceArrayInfoFromTransforms();
  auto bindingKind = ir_lowerer::makeBindingKindFromTransforms();
  auto isStringBinding = ir_lowerer::makeIsStringBindingType();
  auto isFileErrorBinding = ir_lowerer::makeIsFileErrorBindingType();
  auto bindingValueKind = ir_lowerer::makeBindingValueKindFromTransforms();

  auto resolveStructTypeName = ir_lowerer::makeResolveStructTypePathFromScope(structNames, importAliases);

  auto resolveUninitializedTypeInfo = ir_lowerer::makeResolveUninitializedTypeInfo(resolveStructTypeName, error);

  using StructArrayInfo = ir_lowerer::StructArrayTypeInfo;
  using StructSlotFieldInfo = ir_lowerer::StructSlotFieldInfo;
  ir_lowerer::StructLayoutFieldIndex structLayoutFieldIndex = ir_lowerer::buildStructLayoutFieldIndex(
      structFieldInfoByName.size(),
      [&](const ir_lowerer::AppendStructLayoutFieldFn &appendStructLayoutField) {
        for (const auto &entry : structFieldInfoByName) {
          for (const auto &field : entry.second) {
            ir_lowerer::StructLayoutFieldInfo info;
            info.name = field.name;
            info.typeName = field.binding.typeName;
            info.typeTemplateArg = field.binding.typeTemplateArg;
            info.isStatic = field.isStatic;
            appendStructLayoutField(entry.first, info);
          }
        }
      });

  auto resolveStructArrayInfoFromPath = ir_lowerer::makeResolveStructArrayTypeInfoFromLayoutFieldIndex(
      structLayoutFieldIndex, valueKindFromTypeName);

  auto applyStructArrayInfo = ir_lowerer::makeApplyStructArrayInfoFromBindingWithLayoutFieldIndex(
      resolveStructTypeName, structLayoutFieldIndex, valueKindFromTypeName);

  ir_lowerer::StructSlotLayoutCache structSlotLayoutCache;
  std::unordered_set<std::string> structSlotLayoutStack;
  using StructSlotLayout = ir_lowerer::StructSlotLayoutInfo;
  auto resolveStructSlotLayout = ir_lowerer::makeResolveStructSlotLayoutFromDefinitionFieldIndex(
      structLayoutFieldIndex,
      defMap,
      resolveStructTypeName,
      valueKindFromTypeName,
      structSlotLayoutCache,
      structSlotLayoutStack,
      error);

  auto resolveStructFieldSlot = ir_lowerer::makeResolveStructFieldSlotFromDefinitionFieldIndex(
      structLayoutFieldIndex,
      defMap,
      resolveStructTypeName,
      valueKindFromTypeName,
      structSlotLayoutCache,
      structSlotLayoutStack,
      error);

  using UninitializedStorageAccess = ir_lowerer::UninitializedStorageAccessInfo;
  ir_lowerer::UninitializedFieldBindingIndex uninitializedFieldBindingIndex =
      ir_lowerer::buildUninitializedFieldBindingIndexFromStructLayoutFieldIndex(structLayoutFieldIndex);

  auto resolveUninitializedStorage = ir_lowerer::makeResolveUninitializedStorageAccessFromDefinitionFieldIndex(
      uninitializedFieldBindingIndex, defMap, resolveUninitializedTypeInfo, resolveStructFieldSlot, error);

  auto applyStructValueInfo = ir_lowerer::makeApplyStructValueInfoFromBinding(resolveStructTypeName);

  auto inferStructExprPath = ir_lowerer::makeInferStructExprPathFromDefinitionMapByCallTargetWithFieldIndex(
      defMap, resolveStructTypeName, resolveExprPath, uninitializedFieldBindingIndex, resolveStructFieldSlot);


  auto combineNumericKinds = ir_lowerer::makeCombineNumericKinds();
