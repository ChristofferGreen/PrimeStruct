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
  const bool hasMathImport = ir_lowerer::hasProgramMathImport(program.imports);

  ir_lowerer::EntryReturnRuntimeEntrySetupMathTypeStructAndUninitializedResolutionSetup
      entryReturnRuntimeEntrySetupMathTypeStructAndUninitializedResolutionSetup;
  if (!ir_lowerer::buildProgramEntryReturnRuntimeEntrySetupMathTypeStructAndUninitializedResolutionSetup(
      stringTable,
      function,
      program,
      *entryDef,
      entryPath,
      defMap,
      importAliases,
      structNames,
      structFieldInfoByName.size(),
      [&](const ir_lowerer::AppendStructLayoutFieldFn &appendStructLayoutField) {
        for (const auto &entry : structFieldInfoByName) {
          auto defIt = defMap.find(entry.first);
          if (defIt == defMap.end() || defIt->second == nullptr) {
            continue;
          }
          const Definition &structDef = *defIt->second;
          std::size_t fieldIndex = 0;
          for (const auto &fieldStmt : structDef.statements) {
            if (!fieldStmt.isBinding || fieldIndex >= entry.second.size()) {
              continue;
            }
            const auto &field = entry.second[fieldIndex++];
            ir_lowerer::StructLayoutFieldInfo info;
            info.name = fieldStmt.name;
            info.typeName = field.typeName;
            info.typeTemplateArg = field.typeTemplateArg;
            info.isStatic = ir_lowerer::isStaticField(fieldStmt);
            appendStructLayoutField(entry.first, info);
          }
        }
      },
      entryReturnRuntimeEntrySetupMathTypeStructAndUninitializedResolutionSetup,
      error)) {
    return false;
  }
  const auto &entryReturnConfig =
      entryReturnRuntimeEntrySetupMathTypeStructAndUninitializedResolutionSetup.entryReturnConfig;
  bool returnsVoid = entryReturnConfig.returnsVoid;
  ResultReturnInfo entryResultInfo = entryReturnConfig.resultInfo;
  bool entryHasResultInfo = entryReturnConfig.hasResultInfo;

  const auto &runtimeEntrySetupMathTypeStructAndUninitializedResolutionSetup =
      entryReturnRuntimeEntrySetupMathTypeStructAndUninitializedResolutionSetup
          .runtimeEntrySetupMathTypeStructAndUninitializedResolutionSetup;
  const auto &runtimeErrorAndStringLiteralSetup =
      runtimeEntrySetupMathTypeStructAndUninitializedResolutionSetup
          .runtimeErrorAndStringLiteralSetup;
  const auto &stringLiteralHelpers = runtimeErrorAndStringLiteralSetup.stringLiteralHelpers;
  auto internString = stringLiteralHelpers.internString;

  const auto &runtimeErrorEmitters = runtimeErrorAndStringLiteralSetup.runtimeErrorEmitters;
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

  const auto &entrySetupMathTypeStructAndUninitializedResolutionSetup =
      runtimeEntrySetupMathTypeStructAndUninitializedResolutionSetup
          .entrySetupMathTypeStructAndUninitializedResolutionSetup;
  const auto &entryCountCallOnErrorSetup =
      entrySetupMathTypeStructAndUninitializedResolutionSetup.entryCountCallOnErrorSetup;
  const auto &entryCountAccessSetup = entryCountCallOnErrorSetup.countAccessSetup;
  const bool hasEntryArgs = entryCountAccessSetup.hasEntryArgs;
  const std::string &entryArgsName = entryCountAccessSetup.entryArgsName;
  const auto &countAccessClassifiers = entryCountAccessSetup.classifiers;
  auto isEntryArgsName = countAccessClassifiers.isEntryArgsName;
  auto isArrayCountCall = countAccessClassifiers.isArrayCountCall;
  auto isVectorCapacityCall = countAccessClassifiers.isVectorCapacityCall;

  auto resolveStringTableTarget = stringLiteralHelpers.resolveStringTableTarget;

  auto isStringCountCall = countAccessClassifiers.isStringCountCall;

  const auto &entryCallOnErrorSetup = entryCountCallOnErrorSetup.callOnErrorSetup;
  const auto &callResolutionAdapters = entryCallOnErrorSetup.callResolutionAdapters;
  auto resolveExprPath = callResolutionAdapters.resolveExprPath;
  auto isTailCallCandidate = callResolutionAdapters.isTailCallCandidate;
  if (entryCallOnErrorSetup.hasTailExecution) {
    function.metadata.instrumentationFlags |= InstrumentationTailExecution;
  }
  OnErrorByDefinition onErrorByDef = entryCallOnErrorSetup.onErrorByDefinition;

  const auto &setupMathTypeStructAndUninitializedResolutionSetup =
      entrySetupMathTypeStructAndUninitializedResolutionSetup
          .setupMathTypeStructAndUninitializedResolutionSetup;
  const auto &setupMathAndBindingAdapters =
      setupMathTypeStructAndUninitializedResolutionSetup.setupMathAndBindingAdapters;
  const auto &setupMathResolvers = setupMathAndBindingAdapters.setupMathResolvers;
  auto getMathBuiltinName = setupMathResolvers.getMathBuiltinName;
  auto getMathConstantName = setupMathResolvers.getMathConstantName;

  const auto &bindingTypeAdapters = setupMathAndBindingAdapters.bindingTypeAdapters;
  auto setReferenceArrayInfo = bindingTypeAdapters.setReferenceArrayInfo;
  auto bindingKind = bindingTypeAdapters.bindingKind;
  auto isStringBinding = bindingTypeAdapters.isStringBinding;
  auto isFileErrorBinding = bindingTypeAdapters.isFileErrorBinding;
  auto bindingValueKind = bindingTypeAdapters.bindingValueKind;

  const auto &setupTypeStructAndUninitializedResolutionSetup =
      setupMathTypeStructAndUninitializedResolutionSetup.setupTypeStructAndUninitializedResolutionSetup;
  const auto &setupTypeAndStructTypeAdapters =
      setupTypeStructAndUninitializedResolutionSetup.setupTypeAndStructTypeAdapters;
  auto valueKindFromTypeName = setupTypeAndStructTypeAdapters.valueKindFromTypeName;
  const auto &structTypeResolutionAdapters = setupTypeAndStructTypeAdapters.structTypeResolutionAdapters;
  auto resolveStructTypeName = structTypeResolutionAdapters.resolveStructTypeName;

  using StructArrayInfo = ir_lowerer::StructArrayTypeInfo;
  using StructSlotFieldInfo = ir_lowerer::StructSlotFieldInfo;
  const auto &structAndUninitializedResolutionSetup =
      setupTypeStructAndUninitializedResolutionSetup.structAndUninitializedResolutionSetup;
  const auto &structLayoutResolutionAdapters =
      structAndUninitializedResolutionSetup.structLayoutResolutionAdapters;
  const auto &structArrayInfoAdapters = structLayoutResolutionAdapters.structArrayInfo;
  auto resolveStructArrayInfoFromPath = structArrayInfoAdapters.resolveStructArrayTypeInfoFromPath;
  auto applyStructArrayInfo = structArrayInfoAdapters.applyStructArrayInfo;

  using StructSlotLayout = ir_lowerer::StructSlotLayoutInfo;
  const auto &structSlotResolutionAdapters = structLayoutResolutionAdapters.structSlotResolution;
  auto resolveStructSlotLayout = structSlotResolutionAdapters.resolveStructSlotLayout;
  auto resolveStructFieldSlot = structSlotResolutionAdapters.resolveStructFieldSlot;

  using UninitializedStorageAccess = ir_lowerer::UninitializedStorageAccessInfo;
  const auto &uninitializedResolutionAdapters =
      structAndUninitializedResolutionSetup.uninitializedResolutionAdapters;
  auto resolveUninitializedTypeInfo = uninitializedResolutionAdapters.resolveUninitializedTypeInfo;
  auto resolveUninitializedStorage = uninitializedResolutionAdapters.resolveUninitializedStorage;
  auto inferStructExprPath = uninitializedResolutionAdapters.inferStructExprPath;

  auto applyStructValueInfo = structTypeResolutionAdapters.applyStructValueInfo;


  auto combineNumericKinds = setupTypeAndStructTypeAdapters.combineNumericKinds;
