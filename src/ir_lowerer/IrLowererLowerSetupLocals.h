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
    for (size_t i = 0; i < stringTable.size(); ++i) {
      if (stringTable[i] == text) {
        return static_cast<int32_t>(i);
      }
    }
    stringTable.push_back(text);
    return static_cast<int32_t>(stringTable.size() - 1);
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
  if (!entryDef->parameters.empty()) {
    if (entryDef->parameters.size() != 1) {
      error = "native backend only supports a single array<string> entry parameter";
      return false;
    }
    const Expr &param = entryDef->parameters.front();
    if (!isEntryArgsParam(param)) {
      error = "native backend entry parameter must be array<string>";
      return false;
    }
    if (!param.args.empty()) {
      error = "native backend does not allow entry parameter defaults";
      return false;
    }
    hasEntryArgs = true;
    entryArgsName = param.name;
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
    return defMap.find(path) != defMap.end();
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
  struct StructSlotFieldInfo {
    std::string name;
    int32_t slotOffset = -1;
    int32_t slotCount = 0;
    LocalInfo::ValueKind valueKind = LocalInfo::ValueKind::Unknown;
    std::string structPath;
  };
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
    auto defIt = defMap.find(structPath);
    if (defIt == defMap.end() || !defIt->second) {
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
    const std::string &namespacePrefix = defIt->second->namespacePrefix;
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
    StructSlotLayout layout;
    if (!resolveStructSlotLayout(structPath, layout)) {
      return false;
    }
    for (const auto &field : layout.fields) {
      if (field.name == fieldName) {
        out = field;
        return true;
      }
    }
    return false;
  };

  struct UninitializedStorageAccess {
    enum class Location { Local, Field } location = Location::Local;
    const LocalInfo *local = nullptr;
    const LocalInfo *receiver = nullptr;
    StructSlotFieldInfo fieldSlot;
    UninitializedTypeInfo typeInfo;
  };

  auto resolveUninitializedStorage = [&](const Expr &storage,
                                         const LocalMap &localsIn,
                                         UninitializedStorageAccess &out,
                                         bool &resolved) -> bool {
    resolved = false;
    if (storage.kind == Expr::Kind::Name) {
      auto it = localsIn.find(storage.name);
      if (it != localsIn.end() && it->second.isUninitializedStorage) {
        out = UninitializedStorageAccess{};
        out.location = UninitializedStorageAccess::Location::Local;
        out.local = &it->second;
        out.typeInfo.kind = it->second.kind;
        out.typeInfo.valueKind = it->second.valueKind;
        out.typeInfo.mapKeyKind = it->second.mapKeyKind;
        out.typeInfo.mapValueKind = it->second.mapValueKind;
        out.typeInfo.structPath = it->second.structTypeName;
        resolved = true;
      }
      return true;
    }
    if (!storage.isFieldAccess || storage.args.size() != 1) {
      return true;
    }
    const Expr &receiver = storage.args.front();
    if (receiver.kind != Expr::Kind::Name) {
      return true;
    }
    auto recvIt = localsIn.find(receiver.name);
    if (recvIt == localsIn.end() || recvIt->second.structTypeName.empty()) {
      return true;
    }
    const std::string &structPath = recvIt->second.structTypeName;
    auto fieldIt = structFieldInfoByName.find(structPath);
    if (fieldIt == structFieldInfoByName.end()) {
      return true;
    }
    for (const auto &field : fieldIt->second) {
      if (field.isStatic) {
        continue;
      }
      if (field.name != storage.name) {
        continue;
      }
      if (field.binding.typeName != "uninitialized" || field.binding.typeTemplateArg.empty()) {
        return true;
      }
      UninitializedTypeInfo info;
      auto defIt = defMap.find(structPath);
      std::string namespacePrefix;
      if (defIt != defMap.end() && defIt->second) {
        namespacePrefix = defIt->second->namespacePrefix;
      }
      if (!resolveUninitializedTypeInfo(field.binding.typeTemplateArg, namespacePrefix, info)) {
        if (error.empty()) {
          error = "native backend does not support uninitialized storage for type: " +
                  field.binding.typeTemplateArg;
        }
        return false;
      }
      StructSlotFieldInfo slot;
      if (!resolveStructFieldSlot(structPath, field.name, slot)) {
        return false;
      }
      out = UninitializedStorageAccess{};
      out.location = UninitializedStorageAccess::Location::Field;
      out.receiver = &recvIt->second;
      out.fieldSlot = slot;
      out.typeInfo = info;
      resolved = true;
      return true;
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
    if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall || candidate.isFieldAccess) {
      return "";
    }
    std::string resolved = resolveExprPath(candidate);
    if (structFieldInfoByName.count(resolved) > 0) {
      return resolved;
    }
    return inferDefinitionStructReturnPathImpl(resolved, visitedDefs);
  };
  inferDefinitionStructReturnPathImpl = [&](const std::string &defPath,
                                            std::unordered_set<std::string> &visitedDefs) -> std::string {
    if (defPath.empty()) {
      return "";
    }
    if (!visitedDefs.insert(defPath).second) {
      return "";
    }
    auto defIt = defMap.find(defPath);
    if (defIt == defMap.end() || !defIt->second) {
      return "";
    }
    const Definition &def = *defIt->second;
    for (const auto &transform : def.transforms) {
      if (transform.name != "return" || transform.templateArgs.size() != 1) {
        continue;
      }
      std::string resolved;
      if (resolveStructTypeName(transform.templateArgs.front(), def.namespacePrefix, resolved)) {
        return resolved;
      }
      break;
    }
    for (const auto &transform : def.transforms) {
      if (transform.name == "effects" || transform.name == "capabilities") {
        continue;
      }
      if (transform.name == "return") {
        continue;
      }
      if (isBindingQualifierName(transform.name)) {
        continue;
      }
      if (!transform.arguments.empty()) {
        continue;
      }
      std::string resolved;
      if (resolveStructTypeName(transform.name, def.namespacePrefix, resolved)) {
        return resolved;
      }
    }
    if (def.returnExpr.has_value()) {
      std::string inferred = inferStructReturnExprPath(*def.returnExpr, visitedDefs);
      if (!inferred.empty()) {
        return inferred;
      }
    }
    for (const auto &stmt : def.statements) {
      if (!isReturnCall(stmt) || stmt.args.size() != 1) {
        continue;
      }
      std::string inferred = inferStructReturnExprPath(stmt.args.front(), visitedDefs);
      if (!inferred.empty()) {
        return inferred;
      }
    }
    return "";
  };
  auto inferDefinitionStructReturnPath = [&](const std::string &defPath) -> std::string {
    std::unordered_set<std::string> visitedDefs;
    return inferDefinitionStructReturnPathImpl(defPath, visitedDefs);
  };
  inferStructExprPath = [&](const Expr &expr, const LocalMap &localsIn) -> std::string {
    if (expr.kind == Expr::Kind::Name) {
      auto it = localsIn.find(expr.name);
      if (it != localsIn.end()) {
        return it->second.structTypeName;
      }
      return "";
    }
    if (expr.kind == Expr::Kind::Call) {
      if (expr.isFieldAccess && expr.args.size() == 1) {
        std::string receiverStruct = inferStructExprPath(expr.args.front(), localsIn);
        if (receiverStruct.empty()) {
          return "";
        }
        StructSlotFieldInfo fieldInfo;
        if (!resolveStructFieldSlot(receiverStruct, expr.name, fieldInfo)) {
          return "";
        }
        return fieldInfo.structPath;
      }
      if (!expr.isMethodCall) {
        std::string resolved = resolveExprPath(expr);
        if (structFieldInfoByName.count(resolved) > 0) {
          return resolved;
        }
        std::string returnStruct = inferDefinitionStructReturnPath(resolved);
        if (!returnStruct.empty()) {
          return returnStruct;
        }
      }
    }
    return "";
  };


  auto combineNumericKinds = [&](LocalInfo::ValueKind left,
                                 LocalInfo::ValueKind right) -> LocalInfo::ValueKind {
    return ir_lowerer::combineNumericKinds(left, right);
  };
