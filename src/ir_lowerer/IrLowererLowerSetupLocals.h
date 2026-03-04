  const auto setupTypeAdapters = ir_lowerer::makeSetupTypeAdapters();
  auto valueKindFromTypeName = setupTypeAdapters.valueKindFromTypeName;

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

  const auto callResolutionAdapters = ir_lowerer::makeCallResolutionAdapters(defMap, importAliases);
  auto resolveExprPath = callResolutionAdapters.resolveExprPath;
  auto isTailCallCandidate = callResolutionAdapters.isTailCallCandidate;
  const bool sawTailExecution =
      ir_lowerer::hasTailExecutionCandidate(entryDef->statements, returnsVoid, isTailCallCandidate);
  if (sawTailExecution) {
    function.metadata.instrumentationFlags |= InstrumentationTailExecution;
  }
  auto definitionExists = callResolutionAdapters.definitionExists;
  OnErrorByDefinition onErrorByDef;
  if (!ir_lowerer::buildOnErrorByDefinition(program, resolveExprPath, definitionExists, onErrorByDef, error)) {
    return false;
  }

  const auto setupMathResolvers = ir_lowerer::makeSetupMathResolvers(hasMathImport);
  auto getMathBuiltinName = setupMathResolvers.getMathBuiltinName;
  auto getMathConstantName = setupMathResolvers.getMathConstantName;

  const auto bindingTypeAdapters = ir_lowerer::makeBindingTypeAdapters();
  auto setReferenceArrayInfo = bindingTypeAdapters.setReferenceArrayInfo;
  auto bindingKind = bindingTypeAdapters.bindingKind;
  auto isStringBinding = bindingTypeAdapters.isStringBinding;
  auto isFileErrorBinding = bindingTypeAdapters.isFileErrorBinding;
  auto bindingValueKind = bindingTypeAdapters.bindingValueKind;

  const auto structTypeResolutionAdapters = ir_lowerer::makeStructTypeResolutionAdapters(structNames, importAliases);
  auto resolveStructTypeName = structTypeResolutionAdapters.resolveStructTypeName;

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

  const auto structArrayInfoAdapters = ir_lowerer::makeStructArrayInfoAdapters(
      structLayoutFieldIndex, resolveStructTypeName, valueKindFromTypeName);
  auto resolveStructArrayInfoFromPath = structArrayInfoAdapters.resolveStructArrayTypeInfoFromPath;
  auto applyStructArrayInfo = structArrayInfoAdapters.applyStructArrayInfo;

  ir_lowerer::StructSlotLayoutCache structSlotLayoutCache;
  std::unordered_set<std::string> structSlotLayoutStack;
  using StructSlotLayout = ir_lowerer::StructSlotLayoutInfo;
  const auto structSlotResolutionAdapters = ir_lowerer::makeStructSlotResolutionAdapters(
      structLayoutFieldIndex,
      defMap,
      resolveStructTypeName,
      valueKindFromTypeName,
      structSlotLayoutCache,
      structSlotLayoutStack,
      error);
  auto resolveStructSlotLayout = structSlotResolutionAdapters.resolveStructSlotLayout;
  auto resolveStructFieldSlot = structSlotResolutionAdapters.resolveStructFieldSlot;

  using UninitializedStorageAccess = ir_lowerer::UninitializedStorageAccessInfo;
  ir_lowerer::UninitializedFieldBindingIndex uninitializedFieldBindingIndex =
      ir_lowerer::buildUninitializedFieldBindingIndexFromStructLayoutFieldIndex(structLayoutFieldIndex);

  const auto uninitializedResolutionAdapters = ir_lowerer::makeUninitializedResolutionAdapters(
      resolveStructTypeName, resolveExprPath, uninitializedFieldBindingIndex, defMap, resolveStructFieldSlot, error);
  auto resolveUninitializedTypeInfo = uninitializedResolutionAdapters.resolveUninitializedTypeInfo;
  auto resolveUninitializedStorage = uninitializedResolutionAdapters.resolveUninitializedStorage;
  auto inferStructExprPath = uninitializedResolutionAdapters.inferStructExprPath;

  auto applyStructValueInfo = structTypeResolutionAdapters.applyStructValueInfo;


  auto combineNumericKinds = setupTypeAdapters.combineNumericKinds;
