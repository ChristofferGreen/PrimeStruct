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
  std::unordered_map<std::string, std::vector<InstructionSourceRange>> instructionSourceRangesByFunction;
  std::unordered_map<std::string, FunctionSyntaxProvenance> functionSyntaxProvenanceByName;
  functionSyntaxProvenanceByName.reserve(defMap.size());
  for (const auto &[path, definition] : defMap) {
    FunctionSyntaxProvenance provenance;
    if (definition != nullptr) {
      if (definition->sourceLine > 0) {
        provenance.line = static_cast<uint32_t>(definition->sourceLine);
      }
      if (definition->sourceColumn > 0) {
        provenance.column = static_cast<uint32_t>(definition->sourceColumn);
      }
    }
    functionSyntaxProvenanceByName.insert_or_assign(path, provenance);
  }
  auto appendInstructionSourceRange = [&](const std::string &functionName,
                                          const Expr &expr,
                                          size_t beginIndex,
                                          size_t endIndex) {
    if (functionName.empty() || endIndex <= beginIndex) {
      return;
    }
    InstructionSourceRange range;
    range.beginIndex = beginIndex;
    range.endIndex = endIndex;
    if (expr.sourceLine > 0) {
      range.line = static_cast<uint32_t>(expr.sourceLine);
    }
    if (expr.sourceColumn > 0) {
      range.column = static_cast<uint32_t>(expr.sourceColumn);
    }
    instructionSourceRangesByFunction[functionName].push_back(range);
  };
  std::vector<std::vector<int32_t>> fileScopeStack;
  std::optional<OnErrorHandler> currentOnError;
  std::optional<ResultReturnInfo> currentReturnResult;
  const bool hasMathImport = ir_lowerer::hasProgramMathImport(program.imports);

  ir_lowerer::SetupLocalsOrchestration setupLocalsOrchestration{};
  if (!ir_lowerer::runLowerLocalsSetup(stringTable,
                                       function,
                                       program,
                                       semanticProgram,
                                       *entryDef,
                                       entryPath,
                                       defMap,
                                       importAliases,
                                       structNames,
                                       structFieldInfoByName,
                                       setupLocalsOrchestration,
                                       error)) {
    return false;
  }

  const auto &entryReturnConfig = setupLocalsOrchestration.entryReturnConfig;
  bool returnsVoid = entryReturnConfig.returnsVoid;
  ResultReturnInfo entryResultInfo = entryReturnConfig.resultInfo;
  bool entryHasResultInfo = entryReturnConfig.hasResultInfo;

  const auto &runtimeErrorAndStringLiteralSetup = setupLocalsOrchestration.runtimeErrorAndStringLiteralSetup;
  const auto &stringLiteralHelpers = runtimeErrorAndStringLiteralSetup.stringLiteralHelpers;
  auto internString = stringLiteralHelpers.internString;

  const auto &runtimeErrorEmitters = runtimeErrorAndStringLiteralSetup.runtimeErrorEmitters;
  auto emitArrayIndexOutOfBounds = runtimeErrorEmitters.emitArrayIndexOutOfBounds;
  auto emitPointerIndexOutOfBounds = runtimeErrorEmitters.emitPointerIndexOutOfBounds;
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

  const auto &entryCountAccessSetup = setupLocalsOrchestration.entryCountAccessSetup;
  const bool hasEntryArgs = entryCountAccessSetup.hasEntryArgs;
  const std::string &entryArgsName = entryCountAccessSetup.entryArgsName;
  const auto &countAccessClassifiers = entryCountAccessSetup.classifiers;
  auto isEntryArgsName = countAccessClassifiers.isEntryArgsName;
  auto isArrayCountCall = countAccessClassifiers.isArrayCountCall;
  auto isVectorCapacityCall = countAccessClassifiers.isVectorCapacityCall;

  auto resolveStringTableTarget = stringLiteralHelpers.resolveStringTableTarget;

  auto isStringCountCall = countAccessClassifiers.isStringCountCall;

  const auto &entryCallOnErrorSetup = setupLocalsOrchestration.entryCallOnErrorSetup;
  const auto &callResolutionAdapters = entryCallOnErrorSetup.callResolutionAdapters;
  auto resolveExprPath = callResolutionAdapters.resolveExprPath;
  auto isTailCallCandidate = callResolutionAdapters.isTailCallCandidate;
  if (entryCallOnErrorSetup.hasTailExecution) {
    function.metadata.instrumentationFlags |= InstrumentationTailExecution;
  }
  OnErrorByDefinition onErrorByDef = entryCallOnErrorSetup.onErrorByDefinition;

  const auto &setupMathResolvers = setupLocalsOrchestration.setupMathResolvers;
  auto getMathBuiltinName = setupMathResolvers.getMathBuiltinName;
  auto getMathConstantName = setupMathResolvers.getMathConstantName;

  const auto &bindingTypeAdapters = setupLocalsOrchestration.bindingTypeAdapters;
  auto setReferenceArrayInfo = bindingTypeAdapters.setReferenceArrayInfo;
  auto bindingKind = bindingTypeAdapters.bindingKind;
  auto hasExplicitBindingTypeTransform = bindingTypeAdapters.hasExplicitBindingTypeTransform;
  if (!hasExplicitBindingTypeTransform) {
    hasExplicitBindingTypeTransform = [](const Expr &expr) {
      return ir_lowerer::hasExplicitBindingTypeTransform(expr);
    };
  }
  auto isStringBinding = bindingTypeAdapters.isStringBinding;
  auto isFileErrorBinding = bindingTypeAdapters.isFileErrorBinding;
  auto bindingValueKind = bindingTypeAdapters.bindingValueKind;

  const auto &setupTypeAndStructTypeAdapters = setupLocalsOrchestration.setupTypeAndStructTypeAdapters;
  auto valueKindFromTypeName = setupTypeAndStructTypeAdapters.valueKindFromTypeName;
  const auto &structTypeResolutionAdapters = setupTypeAndStructTypeAdapters.structTypeResolutionAdapters;
  auto resolveStructTypeName = structTypeResolutionAdapters.resolveStructTypeName;

  using StructSlotFieldInfo = ir_lowerer::StructSlotFieldInfo;
  const auto &structArrayInfoAdapters = setupLocalsOrchestration.structArrayInfoAdapters;
  auto resolveStructArrayInfoFromPath = structArrayInfoAdapters.resolveStructArrayTypeInfoFromPath;
  auto applyStructArrayInfo = structArrayInfoAdapters.applyStructArrayInfo;

  using StructSlotLayout = ir_lowerer::StructSlotLayoutInfo;
  const auto &structSlotResolutionAdapters = setupLocalsOrchestration.structSlotResolutionAdapters;
  auto resolveStructSlotLayout = structSlotResolutionAdapters.resolveStructSlotLayout;
  auto resolveStructFieldSlot = structSlotResolutionAdapters.resolveStructFieldSlot;

  using UninitializedStorageAccess = ir_lowerer::UninitializedStorageAccessInfo;
  const auto &uninitializedResolutionAdapters = setupLocalsOrchestration.uninitializedResolutionAdapters;
  auto resolveUninitializedTypeInfo = uninitializedResolutionAdapters.resolveUninitializedTypeInfo;
  auto resolveUninitializedStorage = uninitializedResolutionAdapters.resolveUninitializedStorage;
  auto inferStructExprPath = uninitializedResolutionAdapters.inferStructExprPath;

  auto applyStructValueInfo = setupLocalsOrchestration.applyStructValueInfo;


  auto combineNumericKinds = setupTypeAndStructTypeAdapters.combineNumericKinds;
