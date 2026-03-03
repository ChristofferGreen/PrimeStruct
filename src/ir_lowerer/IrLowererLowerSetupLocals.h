  auto valueKindFromTypeName = [&](const std::string &name) -> LocalInfo::ValueKind {
    return ir_lowerer::valueKindFromTypeName(name);
  };

  struct UninitializedTypeInfo {
    LocalInfo::Kind kind = LocalInfo::Kind::Value;
    LocalInfo::ValueKind valueKind = LocalInfo::ValueKind::Unknown;
    LocalInfo::ValueKind mapKeyKind = LocalInfo::ValueKind::Unknown;
    LocalInfo::ValueKind mapValueKind = LocalInfo::ValueKind::Unknown;
    std::string structPath;
  };


  bool hasMathImport = false;
  auto isMathImport = [](const std::string &path) -> bool {
    if (path == "/std/math/*") {
      return true;
    }
    return path.rfind("/std/math/", 0) == 0 && path.size() > 10;
  };
  for (const auto &importPath : program.imports) {
    if (isMathImport(importPath)) {
      hasMathImport = true;
      break;
    }
  }

  bool hasReturnTransform = false;
  bool returnsVoid = false;
  ResultReturnInfo entryResultInfo;
  bool entryHasResultInfo = false;
  for (const auto &transform : entryDef->transforms) {
    if (transform.name != "return") {
      continue;
    }
    hasReturnTransform = true;
    if (transform.templateArgs.size() == 1 && transform.templateArgs.front() == "void") {
      returnsVoid = true;
    }
    if (transform.templateArgs.size() == 1) {
      const std::string &typeName = transform.templateArgs.front();
      std::string base;
      std::string arg;
      if (splitTemplateTypeName(typeName, base, arg) && base == "array") {
        if (valueKindFromTypeName(trimTemplateTypeText(arg)) == LocalInfo::ValueKind::String) {
          error = "native backend does not support string array return types on " + entryPath;
          return false;
        }
      }
      bool resultHasValue = false;
      LocalInfo::ValueKind resultValueKind = LocalInfo::ValueKind::Unknown;
      std::string resultErrorType;
      if (parseResultTypeName(typeName, resultHasValue, resultValueKind, resultErrorType)) {
        entryResultInfo.isResult = true;
        entryResultInfo.hasValue = resultHasValue;
        entryHasResultInfo = true;
      }
    }
  }
  if (!hasReturnTransform && !entryDef->returnExpr.has_value()) {
    returnsVoid = true;
  }

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
    stringIndexOut = -1;
    lengthOut = 0;
    if (expr.kind == Expr::Kind::StringLiteral) {
      std::string decoded;
      if (!ir_lowerer::parseLowererStringLiteral(expr.stringValue, decoded, error)) {
        return false;
      }
      stringIndexOut = internString(decoded);
      lengthOut = decoded.size();
      return true;
    }
    if (expr.kind == Expr::Kind::Name) {
      auto it = localsIn.find(expr.name);
      if (it == localsIn.end()) {
        return false;
      }
      if (it->second.valueKind != LocalInfo::ValueKind::String ||
          it->second.stringSource != LocalInfo::StringSource::TableIndex) {
        return false;
      }
      if (it->second.stringIndex < 0) {
        error = "native backend missing string table data for: " + expr.name;
        return false;
      }
      if (static_cast<size_t>(it->second.stringIndex) >= stringTable.size()) {
        error = "native backend encountered invalid string table index";
        return false;
      }
      stringIndexOut = it->second.stringIndex;
      lengthOut = stringTable[static_cast<size_t>(stringIndexOut)].size();
      return true;
    }
    return false;
  };

  auto isStringCountCall = [&](const Expr &expr, const LocalMap &localsIn) -> bool {
    return ir_lowerer::isStringCountCall(expr, localsIn);
  };

  auto resolveExprPath = [&](const Expr &expr) -> std::string {
    if (!expr.name.empty() && expr.name[0] == '/') {
      return expr.name;
    }
    if (!expr.namespacePrefix.empty()) {
      std::string scoped = expr.namespacePrefix + "/" + expr.name;
      if (defMap.count(scoped) > 0) {
        return scoped;
      }
      auto importIt = importAliases.find(expr.name);
      if (importIt != importAliases.end()) {
        return importIt->second;
      }
      return scoped;
    }
    auto importIt = importAliases.find(expr.name);
    if (importIt != importAliases.end()) {
      return importIt->second;
    }
    return "/" + expr.name;
  };
  auto isTailCallCandidate = [&](const Expr &expr) -> bool {
    if (expr.kind != Expr::Kind::Call || expr.isMethodCall) {
      return false;
    }
    const std::string targetPath = resolveExprPath(expr);
    return defMap.find(targetPath) != defMap.end();
  };
  bool sawTailExecution = false;
  if (!entryDef->statements.empty()) {
    const Expr &lastStmt = entryDef->statements.back();
    if (isReturnCall(lastStmt) && lastStmt.args.size() == 1) {
      sawTailExecution = isTailCallCandidate(lastStmt.args.front());
    } else if (returnsVoid && isTailCallCandidate(lastStmt)) {
      sawTailExecution = true;
    }
  }
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
    if (info.kind != LocalInfo::Kind::Reference) {
      return;
    }
    for (const auto &transform : expr.transforms) {
      if (transform.name != "Reference" || transform.templateArgs.size() != 1) {
        continue;
      }
      std::string base;
      std::string arg;
      if (!splitTemplateTypeName(transform.templateArgs.front(), base, arg) || base != "array") {
        return;
      }
      info.referenceToArray = true;
      if (info.valueKind == LocalInfo::ValueKind::Unknown) {
        info.valueKind = valueKindFromTypeName(trimTemplateTypeText(arg));
      }
      return;
    }
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

  auto joinTemplateArgsLocals = [](const std::vector<std::string> &args) {
    std::string out;
    for (size_t i = 0; i < args.size(); ++i) {
      if (i > 0) {
        out += ", ";
      }
      out += args[i];
    }
    return out;
  };

  auto resolveStructTypeName = [&](const std::string &typeName,
                                   const std::string &namespacePrefix,
                                   std::string &resolvedOut) -> bool {
    if (typeName.empty()) {
      return false;
    }
    if (!typeName.empty() && typeName[0] == '/') {
      if (structNames.count(typeName) > 0) {
        resolvedOut = typeName;
        return true;
      }
      return false;
    }
    if (!namespacePrefix.empty()) {
      if (namespacePrefix.size() > typeName.size() &&
          namespacePrefix.compare(namespacePrefix.size() - typeName.size() - 1, typeName.size() + 1,
                                  "/" + typeName) == 0) {
        if (structNames.count(namespacePrefix) > 0) {
          resolvedOut = namespacePrefix;
          return true;
        }
      }
      std::string candidate = namespacePrefix + "/" + typeName;
      if (structNames.count(candidate) > 0) {
        resolvedOut = candidate;
        return true;
      }
    }
    auto importIt = importAliases.find(typeName);
    if (importIt != importAliases.end() && structNames.count(importIt->second) > 0) {
      resolvedOut = importIt->second;
      return true;
    }
    std::string root = "/" + typeName;
    if (structNames.count(root) > 0) {
      resolvedOut = root;
      return true;
    }
    return false;
  };

  auto resolveUninitializedTypeInfo = [&](const std::string &typeText,
                                          const std::string &namespacePrefix,
                                          UninitializedTypeInfo &out) -> bool {
    out = UninitializedTypeInfo{};
    if (typeText.empty()) {
      return false;
    }
    auto isSupportedNumeric = [&](LocalInfo::ValueKind kind) {
      return kind == LocalInfo::ValueKind::Int32 || kind == LocalInfo::ValueKind::Int64 ||
             kind == LocalInfo::ValueKind::UInt64 || kind == LocalInfo::ValueKind::Float32 ||
             kind == LocalInfo::ValueKind::Float64 || kind == LocalInfo::ValueKind::Bool;
    };

    std::string base;
    std::string argText;
    if (splitTemplateTypeName(typeText, base, argText)) {
      if (base == "array" || base == "vector") {
        std::vector<std::string> args;
        if (!splitTemplateArgs(argText, args) || args.size() != 1) {
          error = "native backend requires " + base + " to have exactly one template argument";
          return false;
        }
        LocalInfo::ValueKind elemKind = valueKindFromTypeName(trimTemplateTypeText(args.front()));
        if (!isSupportedNumeric(elemKind)) {
          error = "native backend only supports numeric/bool uninitialized " + base + " storage";
          return false;
        }
        out.kind = (base == "array") ? LocalInfo::Kind::Array : LocalInfo::Kind::Vector;
        out.valueKind = elemKind;
        return true;
      }
      if (base == "map") {
        std::vector<std::string> args;
        if (!splitTemplateArgs(argText, args) || args.size() != 2) {
          error = "native backend requires map to have exactly two template arguments";
          return false;
        }
        LocalInfo::ValueKind keyKind = valueKindFromTypeName(trimTemplateTypeText(args.front()));
        LocalInfo::ValueKind valueKind = valueKindFromTypeName(trimTemplateTypeText(args.back()));
        if (keyKind == LocalInfo::ValueKind::Unknown || valueKind == LocalInfo::ValueKind::Unknown ||
            valueKind == LocalInfo::ValueKind::String) {
          error = "native backend only supports numeric/bool map values for uninitialized storage";
          return false;
        }
        out.kind = LocalInfo::Kind::Map;
        out.valueKind = valueKind;
        out.mapKeyKind = keyKind;
        out.mapValueKind = valueKind;
        return true;
      }
      if (base == "Pointer" || base == "Reference" || base == "Buffer") {
        out.kind = LocalInfo::Kind::Value;
        out.valueKind = LocalInfo::ValueKind::Int64;
        return true;
      }
      error = "native backend does not support uninitialized storage for type: " + typeText;
      return false;
    }

    LocalInfo::ValueKind kind = valueKindFromTypeName(typeText);
    if (isSupportedNumeric(kind)) {
      out.kind = LocalInfo::Kind::Value;
      out.valueKind = kind;
      return true;
    }
    std::string resolved;
    if (resolveStructTypeName(typeText, namespacePrefix, resolved)) {
      out.kind = LocalInfo::Kind::Value;
      out.structPath = resolved;
      return true;
    }
    if (kind == LocalInfo::ValueKind::String) {
      out.kind = LocalInfo::Kind::Value;
      out.valueKind = kind;
      return true;
    }
    return false;
  };

  struct StructArrayInfo {
    std::string structPath;
    LocalInfo::ValueKind elementKind = LocalInfo::ValueKind::Unknown;
    int32_t fieldCount = 0;
  };
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

  auto resolveStructArrayInfo = [&](const Expr &expr, StructArrayInfo &out) -> bool {
    out = StructArrayInfo{};
    std::string typeName;
    std::string templateArg;
    for (const auto &transform : expr.transforms) {
      if (transform.name == "effects" || transform.name == "capabilities") {
        continue;
      }
      if (isBindingQualifierName(transform.name)) {
        continue;
      }
      if (!transform.arguments.empty()) {
        continue;
      }
      typeName = transform.name;
      if (!transform.templateArgs.empty()) {
        templateArg = joinTemplateArgsLocals(transform.templateArgs);
      }
      break;
    }
    if (typeName == "Reference" || typeName == "Pointer") {
      return false;
    }
    std::string resolved;
    if (!resolveStructTypeName(typeName, expr.namespacePrefix, resolved)) {
      return false;
    }
    auto fieldIt = structFieldInfoByName.find(resolved);
    if (fieldIt == structFieldInfoByName.end()) {
      return false;
    }
    LocalInfo::ValueKind elementKind = LocalInfo::ValueKind::Unknown;
    int32_t fieldCount = 0;
    for (const auto &field : fieldIt->second) {
      if (field.isStatic) {
        continue;
      }
      if (!field.binding.typeTemplateArg.empty()) {
        return false;
      }
      LocalInfo::ValueKind kind = valueKindFromTypeName(field.binding.typeName);
      if (kind == LocalInfo::ValueKind::Unknown || kind == LocalInfo::ValueKind::String) {
        return false;
      }
      if (elementKind == LocalInfo::ValueKind::Unknown) {
        elementKind = kind;
      } else if (elementKind != kind) {
        return false;
      }
      ++fieldCount;
    }
    if (fieldCount == 0 || elementKind == LocalInfo::ValueKind::Unknown) {
      return false;
    }
    out.structPath = resolved;
    out.elementKind = elementKind;
    out.fieldCount = fieldCount;
    return true;
  };

  auto resolveStructArrayInfoFromPath = [&](const std::string &structPath, StructArrayInfo &out) -> bool {
    out = StructArrayInfo{};
    auto fieldIt = structFieldInfoByName.find(structPath);
    if (fieldIt == structFieldInfoByName.end()) {
      return false;
    }
    LocalInfo::ValueKind elementKind = LocalInfo::ValueKind::Unknown;
    int32_t fieldCount = 0;
    for (const auto &field : fieldIt->second) {
      if (field.isStatic) {
        continue;
      }
      if (!field.binding.typeTemplateArg.empty()) {
        return false;
      }
      LocalInfo::ValueKind kind = valueKindFromTypeName(field.binding.typeName);
      if (kind == LocalInfo::ValueKind::Unknown || kind == LocalInfo::ValueKind::String) {
        return false;
      }
      if (elementKind == LocalInfo::ValueKind::Unknown) {
        elementKind = kind;
      } else if (elementKind != kind) {
        return false;
      }
      ++fieldCount;
    }
    if (fieldCount == 0 || elementKind == LocalInfo::ValueKind::Unknown) {
      return false;
    }
    out.structPath = structPath;
    out.elementKind = elementKind;
    out.fieldCount = fieldCount;
    return true;
  };

  auto applyStructArrayInfo = [&](const Expr &expr, LocalInfo &info) {
    StructArrayInfo structInfo;
    if (resolveStructArrayInfo(expr, structInfo)) {
      info.kind = LocalInfo::Kind::Array;
      info.valueKind = structInfo.elementKind;
      info.structTypeName = structInfo.structPath;
      info.structFieldCount = structInfo.fieldCount;
    }
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
    if (!info.structTypeName.empty()) {
      return;
    }
    std::string typeName;
    std::string typeTemplateArg;
    for (const auto &transform : expr.transforms) {
      if (transform.name == "effects" || transform.name == "capabilities") {
        continue;
      }
      if (isBindingQualifierName(transform.name)) {
        continue;
      }
      if (!transform.arguments.empty()) {
        continue;
      }
      typeName = transform.name;
      if (!transform.templateArgs.empty()) {
        typeTemplateArg = joinTemplateArgsLocals(transform.templateArgs);
      }
      break;
    }
    if (typeName.empty()) {
      return;
    }
    if (typeName == "Reference" || typeName == "Pointer") {
      if (!typeTemplateArg.empty()) {
        std::string resolved;
        if (resolveStructTypeName(typeTemplateArg, expr.namespacePrefix, resolved)) {
          info.structTypeName = resolved;
        }
      }
      return;
    }
    if (info.kind != LocalInfo::Kind::Value) {
      return;
    }
    std::string resolved;
    if (resolveStructTypeName(typeName, expr.namespacePrefix, resolved)) {
      info.structTypeName = resolved;
    }
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
    if (left == LocalInfo::ValueKind::Unknown || right == LocalInfo::ValueKind::Unknown) {
      return LocalInfo::ValueKind::Unknown;
    }
    if (left == LocalInfo::ValueKind::String || right == LocalInfo::ValueKind::String) {
      return LocalInfo::ValueKind::Unknown;
    }
    if (left == LocalInfo::ValueKind::Bool || right == LocalInfo::ValueKind::Bool) {
      return LocalInfo::ValueKind::Unknown;
    }
    if (left == LocalInfo::ValueKind::Float32 || right == LocalInfo::ValueKind::Float32 ||
        left == LocalInfo::ValueKind::Float64 || right == LocalInfo::ValueKind::Float64) {
      if (left == LocalInfo::ValueKind::Float32 && right == LocalInfo::ValueKind::Float32) {
        return LocalInfo::ValueKind::Float32;
      }
      if (left == LocalInfo::ValueKind::Float64 && right == LocalInfo::ValueKind::Float64) {
        return LocalInfo::ValueKind::Float64;
      }
      return LocalInfo::ValueKind::Unknown;
    }
    if (left == LocalInfo::ValueKind::UInt64 || right == LocalInfo::ValueKind::UInt64) {
      return (left == LocalInfo::ValueKind::UInt64 && right == LocalInfo::ValueKind::UInt64)
                 ? LocalInfo::ValueKind::UInt64
                 : LocalInfo::ValueKind::Unknown;
    }
    if (left == LocalInfo::ValueKind::Int64 || right == LocalInfo::ValueKind::Int64) {
      if ((left == LocalInfo::ValueKind::Int64 || left == LocalInfo::ValueKind::Int32) &&
          (right == LocalInfo::ValueKind::Int64 || right == LocalInfo::ValueKind::Int32)) {
        return LocalInfo::ValueKind::Int64;
      }
      return LocalInfo::ValueKind::Unknown;
    }
    if (left == LocalInfo::ValueKind::Int32 && right == LocalInfo::ValueKind::Int32) {
      return LocalInfo::ValueKind::Int32;
    }
    return LocalInfo::ValueKind::Unknown;
  };
