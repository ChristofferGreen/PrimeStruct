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
  auto isEntryArgsName = [&](const Expr &expr, const LocalMap &localsIn) -> bool {
    return ir_lowerer::isEntryArgsName(expr, localsIn, hasEntryArgs, entryArgsName);
  };
  auto isArrayCountCall = [&](const Expr &expr, const LocalMap &localsIn) -> bool {
    return ir_lowerer::isArrayCountCall(expr, localsIn, hasEntryArgs, entryArgsName);
  };
  auto isVectorCapacityCall = [&](const Expr &expr, const LocalMap &localsIn) -> bool {
    return ir_lowerer::isVectorCapacityCall(expr, localsIn);
  };

  auto resolveStringTableTarget = [&](const Expr &expr,
                                      const LocalMap &localsIn,
                                      int32_t &stringIndexOut,
                                      size_t &lengthOut) -> bool {
    return ir_lowerer::resolveStringTableTarget(
        expr, localsIn, stringTable, internString, stringIndexOut, lengthOut, error);
  };

  auto isStringCountCall = [&](const Expr &expr, const LocalMap &localsIn) -> bool {
    return ir_lowerer::isStringCountCall(expr, localsIn);
  };

  auto resolveExprPath = [&](const Expr &expr) -> std::string {
    return ir_lowerer::resolveCallPathFromScope(expr, defMap, importAliases);
  };
  auto isTailCallCandidate = [&](const Expr &expr) -> bool {
    return ir_lowerer::isTailCallCandidate(expr, defMap, resolveExprPath);
  };
  const bool sawTailExecution =
      ir_lowerer::hasTailExecutionCandidate(entryDef->statements, returnsVoid, isTailCallCandidate);
  if (sawTailExecution) {
    function.metadata.instrumentationFlags |= InstrumentationTailExecution;
  }
  auto definitionExists = [&](const std::string &path) {
    return ir_lowerer::resolveDefinitionByPath(defMap, path) != nullptr;
  };
  OnErrorByDefinition onErrorByDef;
  if (!ir_lowerer::buildOnErrorByDefinition(program, resolveExprPath, definitionExists, onErrorByDef, error)) {
    return false;
  }

  auto getMathBuiltinName = [&](const Expr &expr, std::string &out) -> bool {
    return ir_lowerer::getSetupMathBuiltinName(expr, hasMathImport, out);
  };

  auto getMathConstantName = [&](const std::string &name, std::string &out) -> bool {
    return ir_lowerer::getSetupMathConstantName(name, hasMathImport, out);
  };

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

  auto resolveStructTypeName = [&](const std::string &typeName,
                                   const std::string &namespacePrefix,
                                   std::string &resolvedOut) -> bool {
    return ir_lowerer::resolveStructTypePathFromScope(
        typeName, namespacePrefix, structNames, importAliases, resolvedOut);
  };

  auto resolveUninitializedTypeInfo = [&](const std::string &typeText,
                                          const std::string &namespacePrefix,
                                          UninitializedTypeInfo &out) -> bool {
    return ir_lowerer::resolveUninitializedTypeInfo(
        typeText, namespacePrefix, resolveStructTypeName, out, error);
  };

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

  auto resolveStructArrayInfoFromPath = [&](const std::string &structPath, StructArrayInfo &out) -> bool {
    return ir_lowerer::resolveStructArrayTypeInfoFromLayoutFieldIndex(
        structPath, structLayoutFieldIndex, valueKindFromTypeName, out);
  };

  auto applyStructArrayInfo = [&](const Expr &expr, LocalInfo &info) {
    ir_lowerer::applyStructArrayInfoFromBindingWithLayoutFieldIndex(
        expr, resolveStructTypeName, structLayoutFieldIndex, valueKindFromTypeName, info);
  };

  ir_lowerer::StructSlotLayoutCache structSlotLayoutCache;
  std::unordered_set<std::string> structSlotLayoutStack;
  using StructSlotLayout = ir_lowerer::StructSlotLayoutInfo;
  auto resolveStructSlotLayout = [&](const std::string &structPath, StructSlotLayout &out) -> bool {
    return ir_lowerer::resolveStructSlotLayoutFromDefinitionFieldIndex(
        structPath,
        structLayoutFieldIndex,
        defMap,
        resolveStructTypeName,
        valueKindFromTypeName,
        structSlotLayoutCache,
        structSlotLayoutStack,
        out,
        error);
  };

  auto resolveStructFieldSlot = [&](const std::string &structPath,
                                    const std::string &fieldName,
                                    StructSlotFieldInfo &out) -> bool {
    return ir_lowerer::resolveStructFieldSlotFromDefinitionFieldIndex(
        structPath,
        fieldName,
        structLayoutFieldIndex,
        defMap,
        resolveStructTypeName,
        valueKindFromTypeName,
        structSlotLayoutCache,
        structSlotLayoutStack,
        out,
        error);
  };

  using UninitializedStorageAccess = ir_lowerer::UninitializedStorageAccessInfo;
  ir_lowerer::UninitializedFieldBindingIndex uninitializedFieldBindingIndex =
      ir_lowerer::buildUninitializedFieldBindingIndexFromStructLayoutFieldIndex(structLayoutFieldIndex);

  auto resolveUninitializedStorage = [&](const Expr &storage,
                                         const LocalMap &localsIn,
                                         UninitializedStorageAccess &out,
                                         bool &resolved) -> bool {
    if (!ir_lowerer::resolveUninitializedStorageAccessFromDefinitionFieldIndex(
            storage,
            localsIn,
            uninitializedFieldBindingIndex,
            defMap,
            resolveUninitializedTypeInfo,
            resolveStructFieldSlot,
            out,
            resolved,
            error)) {
      return false;
    }
    return true;
  };

  auto applyStructValueInfo = [&](const Expr &expr, LocalInfo &info) {
    ir_lowerer::applyStructValueInfoFromBinding(expr, resolveStructTypeName, info);
  };

  auto inferStructExprPath = [&](const Expr &expr, const LocalMap &localsIn) -> std::string {
    return ir_lowerer::inferStructExprPathFromDefinitionMapByCallTargetWithFieldIndex(
        expr,
        localsIn,
        defMap,
        resolveStructTypeName,
        resolveExprPath,
        uninitializedFieldBindingIndex,
        resolveStructFieldSlot);
  };


  auto combineNumericKinds = [&](LocalInfo::ValueKind left,
                                 LocalInfo::ValueKind right) -> LocalInfo::ValueKind {
    return ir_lowerer::combineNumericKinds(left, right);
  };
