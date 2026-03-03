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
  struct StructSlotLayout {
    std::string structPath;
    int32_t totalSlots = 0;
    std::vector<StructSlotFieldInfo> fields;
  };

  auto collectStructArrayFields = [&](const std::string &structPath,
                                      std::vector<ir_lowerer::StructArrayFieldInfo> &out) -> bool {
    out.clear();
    auto fieldIt = structFieldInfoByName.find(structPath);
    if (fieldIt == structFieldInfoByName.end()) {
      return false;
    }
    out.reserve(fieldIt->second.size());
    for (const auto &field : fieldIt->second) {
      ir_lowerer::StructArrayFieldInfo info;
      info.typeName = field.binding.typeName;
      info.typeTemplateArg = field.binding.typeTemplateArg;
      info.isStatic = field.isStatic;
      out.push_back(std::move(info));
    }
    return true;
  };

  auto resolveStructArrayInfoFromPath = [&](const std::string &structPath, StructArrayInfo &out) -> bool {
    return ir_lowerer::resolveStructArrayTypeInfoFromPath(
        structPath, collectStructArrayFields, valueKindFromTypeName, out);
  };

  auto applyStructArrayInfo = [&](const Expr &expr, LocalInfo &info) {
    ir_lowerer::applyStructArrayInfoFromBinding(
        expr, resolveStructTypeName, collectStructArrayFields, valueKindFromTypeName, info);
  };

  std::unordered_map<std::string, StructSlotLayout> structSlotLayoutCache;
  std::unordered_set<std::string> structSlotLayoutStack;
  std::function<bool(const std::string &, StructSlotLayout &)> resolveStructSlotLayout;
  resolveStructSlotLayout = [&](const std::string &structPath, StructSlotLayout &out) -> bool {
    auto cached = structSlotLayoutCache.find(structPath);
    if (cached != structSlotLayoutCache.end()) {
      out = cached->second;
      return true;
    }
    if (!structSlotLayoutStack.insert(structPath).second) {
      error = "recursive struct layout not supported: " + structPath;
      return false;
    }
    const Definition *structDef = ir_lowerer::resolveDefinitionByPath(defMap, structPath);
    if (structDef == nullptr) {
      error = "native backend cannot resolve struct layout: " + structPath;
      structSlotLayoutStack.erase(structPath);
      return false;
    }
    auto fieldIt = structFieldInfoByName.find(structPath);
    if (fieldIt == structFieldInfoByName.end()) {
      error = "native backend cannot resolve struct fields: " + structPath;
      structSlotLayoutStack.erase(structPath);
      return false;
    }
    StructSlotLayout layout;
    layout.structPath = structPath;
    int32_t offset = 1;
    layout.fields.reserve(fieldIt->second.size());
    const std::string &namespacePrefix = structDef->namespacePrefix;
    for (const auto &field : fieldIt->second) {
      if (field.isStatic) {
        continue;
      }
      FieldBinding binding = field.binding;
      if (binding.typeName == "uninitialized") {
        if (binding.typeTemplateArg.empty()) {
          error = "uninitialized requires a template argument on " + structPath;
          structSlotLayoutStack.erase(structPath);
          return false;
        }
        std::string base;
        std::string arg;
        if (splitTemplateTypeName(binding.typeTemplateArg, base, arg)) {
          binding.typeName = base;
          binding.typeTemplateArg = arg;
        } else {
          binding.typeName = binding.typeTemplateArg;
          binding.typeTemplateArg.clear();
        }
      }
      if (!binding.typeTemplateArg.empty()) {
        error = "native backend does not support templated struct fields on " + structPath;
        structSlotLayoutStack.erase(structPath);
        return false;
      }
      StructSlotFieldInfo info;
      info.name = field.name;
      info.slotOffset = offset;
      LocalInfo::ValueKind kind = valueKindFromTypeName(binding.typeName);
      if (kind != LocalInfo::ValueKind::Unknown) {
        info.valueKind = kind;
        info.slotCount = 1;
      } else {
        std::string nestedStruct;
        if (!resolveStructTypeName(binding.typeName, namespacePrefix, nestedStruct)) {
          error = "native backend does not support struct field type: " + binding.typeName + " on " +
                  structPath;
          structSlotLayoutStack.erase(structPath);
          return false;
        }
        StructSlotLayout nestedLayout;
        if (!resolveStructSlotLayout(nestedStruct, nestedLayout)) {
          structSlotLayoutStack.erase(structPath);
          return false;
        }
        info.structPath = nestedStruct;
        info.slotCount = nestedLayout.totalSlots;
      }
      layout.fields.push_back(info);
      offset += info.slotCount;
    }
    layout.totalSlots = offset;
    structSlotLayoutStack.erase(structPath);
    structSlotLayoutCache.emplace(structPath, layout);
    out = std::move(layout);
    return true;
  };

  auto resolveStructFieldSlot = [&](const std::string &structPath,
                                    const std::string &fieldName,
                                    StructSlotFieldInfo &out) -> bool {
    return ir_lowerer::resolveStructFieldSlotFromLayout(
        structPath,
        fieldName,
        [&](const std::string &candidateStructPath, std::vector<StructSlotFieldInfo> &fieldsOut) {
          StructSlotLayout layout;
          if (!resolveStructSlotLayout(candidateStructPath, layout)) {
            return false;
          }
          fieldsOut = layout.fields;
          return true;
        },
        out);
  };

  using UninitializedStorageAccess = ir_lowerer::UninitializedStorageAccessInfo;

  auto resolveUninitializedStorage = [&](const Expr &storage,
                                         const LocalMap &localsIn,
                                         UninitializedStorageAccess &out,
                                         bool &resolved) -> bool {
    if (!ir_lowerer::resolveUninitializedStorageAccessWithFieldBindings(
            storage,
            localsIn,
            [&](const std::string &structPath,
                std::vector<ir_lowerer::UninitializedFieldBindingInfo> &fieldsOut) -> bool {
              auto fieldIt = structFieldInfoByName.find(structPath);
              if (fieldIt == structFieldInfoByName.end()) {
                return false;
              }
              fieldsOut.clear();
              fieldsOut.reserve(fieldIt->second.size());
              for (const auto &field : fieldIt->second) {
                ir_lowerer::UninitializedFieldBindingInfo info;
                info.name = field.name;
                info.typeName = field.binding.typeName;
                info.typeTemplateArg = field.binding.typeTemplateArg;
                info.isStatic = field.isStatic;
                fieldsOut.push_back(std::move(info));
              }
              return true;
            },
            [&](const std::string &candidateStructPath) {
              return ir_lowerer::resolveDefinitionNamespacePrefix(defMap, candidateStructPath);
            },
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

  std::function<std::string(const Expr &, const LocalMap &)> inferStructExprPath;
  std::function<std::string(const std::string &, std::unordered_set<std::string> &)> inferDefinitionStructReturnPathImpl;
  std::function<std::string(const Expr &, std::unordered_set<std::string> &)> inferStructReturnExprPath;
  inferStructReturnExprPath = [&](const Expr &candidate, std::unordered_set<std::string> &visitedDefs) -> std::string {
    return ir_lowerer::inferStructPathFromCallTarget(
        candidate,
        resolveExprPath,
        [&](const std::string &resolvedPath) { return structFieldInfoByName.count(resolvedPath) > 0; },
        [&](const std::string &resolvedPath) {
          return inferDefinitionStructReturnPathImpl(resolvedPath, visitedDefs);
        });
  };
  inferDefinitionStructReturnPathImpl = [&](const std::string &defPath,
                                            std::unordered_set<std::string> &visitedDefs) -> std::string {
    if (defPath.empty()) {
      return "";
    }
    if (!visitedDefs.insert(defPath).second) {
      return "";
    }
    const Definition *resolvedDef = ir_lowerer::resolveDefinitionByPath(defMap, defPath);
    if (resolvedDef == nullptr) {
      return "";
    }
    const Definition &def = *resolvedDef;
    return ir_lowerer::inferStructReturnPathFromDefinition(
        def,
        resolveStructTypeName,
        [&](const Expr &expr) { return inferStructReturnExprPath(expr, visitedDefs); });
  };
  auto inferDefinitionStructReturnPath = [&](const std::string &defPath) -> std::string {
    std::unordered_set<std::string> visitedDefs;
    return inferDefinitionStructReturnPathImpl(defPath, visitedDefs);
  };
  inferStructExprPath = [&](const Expr &expr, const LocalMap &localsIn) -> std::string {
    const std::string nameStructPath = ir_lowerer::inferStructPathFromNameExpr(expr, localsIn);
    if (!nameStructPath.empty() || expr.kind == Expr::Kind::Name) {
      return nameStructPath;
    }
    if (expr.kind == Expr::Kind::Call) {
      const std::string fieldAccessStruct = ir_lowerer::inferStructPathFromFieldAccessCall(
          expr, localsIn, inferStructExprPath, resolveStructFieldSlot);
      if (!fieldAccessStruct.empty() || expr.isFieldAccess) {
        return fieldAccessStruct;
      }
      return ir_lowerer::inferStructPathFromCallTarget(
          expr,
          resolveExprPath,
          [&](const std::string &resolvedPath) { return structFieldInfoByName.count(resolvedPath) > 0; },
          inferDefinitionStructReturnPath);
    }
    return "";
  };


  auto combineNumericKinds = [&](LocalInfo::ValueKind left,
                                 LocalInfo::ValueKind right) -> LocalInfo::ValueKind {
    return ir_lowerer::combineNumericKinds(left, right);
  };
