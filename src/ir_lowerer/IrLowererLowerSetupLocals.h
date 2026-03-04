  auto valueKindFromTypeName = [&](const std::string &name) -> LocalInfo::ValueKind {
    return ir_lowerer::valueKindFromTypeName(name);
  };

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

  auto internString = [&](const std::string &text) -> int32_t {
    return ir_lowerer::internLowererString(text, stringTable);
  };

  auto emitArrayIndexOutOfBounds = [&]() {
    ir_lowerer::emitArrayIndexOutOfBounds(function, internString);
  };

  auto emitStringIndexOutOfBounds = [&]() {
    ir_lowerer::emitStringIndexOutOfBounds(function, internString);
  };

  auto emitMapKeyNotFound = [&]() {
    ir_lowerer::emitMapKeyNotFound(function, internString);
  };

  auto emitVectorIndexOutOfBounds = [&]() {
    ir_lowerer::emitVectorIndexOutOfBounds(function, internString);
  };

  auto emitVectorPopOnEmpty = [&]() {
    ir_lowerer::emitVectorPopOnEmpty(function, internString);
  };

  auto emitVectorCapacityExceeded = [&]() {
    ir_lowerer::emitVectorCapacityExceeded(function, internString);
  };

  auto emitVectorReserveNegative = [&]() {
    ir_lowerer::emitVectorReserveNegative(function, internString);
  };

  auto emitVectorReserveExceeded = [&]() {
    ir_lowerer::emitVectorReserveExceeded(function, internString);
  };

  auto emitLoopCountNegative = [&]() {
    ir_lowerer::emitLoopCountNegative(function, internString);
  };

  auto emitPowNegativeExponent = [&]() {
    ir_lowerer::emitPowNegativeExponent(function, internString);
  };

  auto emitFloatToIntNonFinite = [&]() {
    ir_lowerer::emitFloatToIntNonFinite(function, internString);
  };

  bool hasEntryArgs = false;
  std::string entryArgsName;
  if (!ir_lowerer::resolveEntryArgsParameter(*entryDef, hasEntryArgs, entryArgsName, error)) {
    return false;
  }
  auto isEntryArgsName = ir_lowerer::makeIsEntryArgsName(hasEntryArgs, entryArgsName);
  auto isArrayCountCall = ir_lowerer::makeIsArrayCountCall(hasEntryArgs, entryArgsName);
  auto isVectorCapacityCall = ir_lowerer::makeIsVectorCapacityCall();

  auto resolveStringTableTarget = ir_lowerer::makeResolveStringTableTarget(stringTable, internString, error);

  auto isStringCountCall = ir_lowerer::makeIsStringCountCall();

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

  auto setReferenceArrayInfo = [&](const Expr &expr, LocalInfo &info) {
    ir_lowerer::setReferenceArrayInfoFromTransforms(expr, info);
  };

  auto bindingKind = [&](const Expr &expr) -> LocalInfo::Kind {
    return ir_lowerer::bindingKindFromTransforms(expr);
  };

  auto isStringBinding = [&](const Expr &expr) -> bool {
    return ir_lowerer::isStringBindingType(expr);
  };
  auto isFileErrorBinding = [&](const Expr &expr) -> bool {
    return ir_lowerer::isFileErrorBindingType(expr);
  };

  auto bindingValueKind = [&](const Expr &expr, LocalInfo::Kind kind) -> LocalInfo::ValueKind {
    return ir_lowerer::bindingValueKindFromTransforms(expr, kind);
  };

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


  auto combineNumericKinds = [&](LocalInfo::ValueKind left,
                                 LocalInfo::ValueKind right) -> LocalInfo::ValueKind {
    return ir_lowerer::combineNumericKinds(left, right);
  };
