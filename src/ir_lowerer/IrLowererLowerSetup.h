bool IrLowerer::lower(const Program &program,
                      const std::string &entryPath,
                      IrModule &out,
                      std::string &error) const {
  out = IrModule{};

  const Definition *entryDef = nullptr;
  for (const auto &def : program.definitions) {
    if (def.fullPath == entryPath) {
      entryDef = &def;
      break;
    }
  }
  if (!entryDef) {
    error = "native backend requires entry definition " + entryPath;
    return false;
  }

  auto isSupportedEffect = [](const std::string &name) {
    return name == "io_out" || name == "io_err" || name == "heap_alloc" || name == "pathspace_notify" ||
           name == "pathspace_insert" || name == "pathspace_take";
  };
  for (const auto &def : program.definitions) {
    for (const auto &transform : def.transforms) {
      if (transform.name != "effects") {
        continue;
      }
      for (const auto &effect : transform.arguments) {
        if (!isSupportedEffect(effect)) {
          error = "native backend does not support effect: " + effect + " on " + def.fullPath;
          return false;
        }
      }
    }
  }

  std::unordered_map<std::string, const Definition *> defMap;
  defMap.reserve(program.definitions.size());
  for (const auto &def : program.definitions) {
    defMap.emplace(def.fullPath, &def);
  }

  std::unordered_map<std::string, std::string> importAliases;
  for (const auto &importPath : program.imports) {
    if (importPath.empty() || importPath[0] != '/') {
      continue;
    }
    const std::string prefix = importPath + "/";
    for (const auto &def : program.definitions) {
      if (def.fullPath.rfind(prefix, 0) != 0) {
        continue;
      }
      const std::string remainder = def.fullPath.substr(prefix.size());
      if (remainder.empty() || remainder.find('/') != std::string::npos) {
        continue;
      }
      importAliases.emplace(remainder, def.fullPath);
    }
  }

  bool hasMathImport = false;
  for (const auto &importPath : program.imports) {
    if (importPath == "/math") {
      hasMathImport = true;
      break;
    }
  }

  bool hasReturnTransform = false;
  bool returnsVoid = false;
  for (const auto &transform : entryDef->transforms) {
    if (transform.name != "return") {
      continue;
    }
    hasReturnTransform = true;
    if (transform.templateArgs.size() == 1 && transform.templateArgs.front() == "void") {
      returnsVoid = true;
    }
  }
  if (!hasReturnTransform && !entryDef->returnExpr.has_value()) {
    returnsVoid = true;
  }

  IrFunction function;
  function.name = entryPath;
  bool sawReturn = false;
  struct LocalInfo {
    int32_t index = 0;
    bool isMutable = false;
    enum class Kind { Value, Pointer, Reference, Array, Vector, Map } kind = Kind::Value;
    enum class ValueKind { Unknown, Int32, Int64, UInt64, Bool, String } valueKind = ValueKind::Unknown;
    ValueKind mapKeyKind = ValueKind::Unknown;
    ValueKind mapValueKind = ValueKind::Unknown;
    enum class StringSource { None, TableIndex, ArgvIndex } stringSource = StringSource::None;
    int32_t stringIndex = -1;
    bool argvChecked = true;
  };
  using LocalMap = std::unordered_map<std::string, LocalInfo>;
  LocalMap locals;
  int32_t nextLocal = 0;
  std::vector<std::string> stringTable;

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
    uint64_t flags = encodePrintFlags(true, true);
    int32_t msgIndex = internString("array index out of bounds");
    function.instructions.push_back(
        {IrOpcode::PrintString, encodePrintStringImm(static_cast<uint64_t>(msgIndex), flags)});
    function.instructions.push_back({IrOpcode::PushI32, 3});
    function.instructions.push_back({IrOpcode::ReturnI32, 0});
  };

  auto emitStringIndexOutOfBounds = [&]() {
    uint64_t flags = encodePrintFlags(true, true);
    int32_t msgIndex = internString("string index out of bounds");
    function.instructions.push_back(
        {IrOpcode::PrintString, encodePrintStringImm(static_cast<uint64_t>(msgIndex), flags)});
    function.instructions.push_back({IrOpcode::PushI32, 3});
    function.instructions.push_back({IrOpcode::ReturnI32, 0});
  };

  auto emitMapKeyNotFound = [&]() {
    uint64_t flags = encodePrintFlags(true, true);
    int32_t msgIndex = internString("map key not found");
    function.instructions.push_back(
        {IrOpcode::PrintString, encodePrintStringImm(static_cast<uint64_t>(msgIndex), flags)});
    function.instructions.push_back({IrOpcode::PushI32, 3});
    function.instructions.push_back({IrOpcode::ReturnI32, 0});
  };

  auto normalizeIndexKind = [](LocalInfo::ValueKind kind) {
    return (kind == LocalInfo::ValueKind::Bool) ? LocalInfo::ValueKind::Int32 : kind;
  };

  auto isSupportedIndexKind = [](LocalInfo::ValueKind kind) {
    return kind == LocalInfo::ValueKind::Int32 || kind == LocalInfo::ValueKind::Int64 ||
           kind == LocalInfo::ValueKind::UInt64;
  };

  auto pushZeroForIndex = [&](LocalInfo::ValueKind kind) {
    return (kind == LocalInfo::ValueKind::Int32) ? IrOpcode::PushI32 : IrOpcode::PushI64;
  };

  auto cmpLtForIndex = [&](LocalInfo::ValueKind kind) {
    return (kind == LocalInfo::ValueKind::Int32) ? IrOpcode::CmpLtI32 : IrOpcode::CmpLtI64;
  };

  auto cmpGeForIndex = [&](LocalInfo::ValueKind kind) {
    if (kind == LocalInfo::ValueKind::Int32) {
      return IrOpcode::CmpGeI32;
    }
    if (kind == LocalInfo::ValueKind::Int64) {
      return IrOpcode::CmpGeI64;
    }
    return IrOpcode::CmpGeU64;
  };

  auto pushOneForIndex = [&](LocalInfo::ValueKind kind) {
    return (kind == LocalInfo::ValueKind::Int32) ? IrOpcode::PushI32 : IrOpcode::PushI64;
  };

  auto addForIndex = [&](LocalInfo::ValueKind kind) {
    return (kind == LocalInfo::ValueKind::Int32) ? IrOpcode::AddI32 : IrOpcode::AddI64;
  };

  auto mulForIndex = [&](LocalInfo::ValueKind kind) {
    return (kind == LocalInfo::ValueKind::Int32) ? IrOpcode::MulI32 : IrOpcode::MulI64;
  };

  auto parseStringLiteral = [&](const std::string &text, std::string &decoded) -> bool {
    ParsedStringLiteral parsed;
    if (!parseStringLiteralToken(text, parsed, error)) {
      return false;
    }
    if (parsed.encoding == StringEncoding::Ascii && !isAsciiText(parsed.decoded)) {
      error = "ascii string literal contains non-ASCII characters";
      return false;
    }
    decoded = std::move(parsed.decoded);
    return true;
  };

  auto isBindingMutable = [](const Expr &expr) -> bool {
    for (const auto &transform : expr.transforms) {
      if (transform.name == "mut") {
        return true;
      }
    }
    return false;
  };

  auto isBindingQualifierName = [](const std::string &name) -> bool {
    return name == "public" || name == "private" || name == "package" || name == "static" || name == "mut" ||
           name == "copy" || name == "restrict" || name == "align_bytes" || name == "align_kbytes";
  };

  auto hasExplicitBindingTypeTransform = [&](const Expr &expr) -> bool {
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
      return true;
    }
    return false;
  };

  bool hasEntryArgs = false;
  std::string entryArgsName;
  auto isEntryArgsParam = [&](const Expr &param) -> bool {
    std::string typeName;
    std::string templateArg;
    bool hasTemplateArg = false;
    for (const auto &transform : param.transforms) {
      if (isBindingQualifierName(transform.name)) {
        continue;
      }
      if (!transform.arguments.empty()) {
        continue;
      }
      typeName = transform.name;
      if (transform.templateArgs.size() == 1) {
        templateArg = transform.templateArgs.front();
        hasTemplateArg = true;
      } else {
        hasTemplateArg = false;
      }
    }
    return typeName == "array" && hasTemplateArg && templateArg == "string";
  };
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
    if (!hasEntryArgs || expr.kind != Expr::Kind::Name || expr.name != entryArgsName) {
      return false;
    }
    return localsIn.count(entryArgsName) == 0;
  };
  auto isArrayCountCall = [&](const Expr &expr, const LocalMap &localsIn) -> bool {
    if (!isSimpleCallName(expr, "count") || expr.args.size() != 1) {
      return false;
    }
    const Expr &target = expr.args.front();
    if (isEntryArgsName(target, localsIn)) {
      return true;
    }
    if (target.kind == Expr::Kind::Name) {
      auto it = localsIn.find(target.name);
      return it != localsIn.end() &&
             (it->second.kind == LocalInfo::Kind::Array || it->second.kind == LocalInfo::Kind::Vector ||
              it->second.kind == LocalInfo::Kind::Map);
    }
    if (target.kind == Expr::Kind::Call) {
      std::string collection;
      if (!getBuiltinCollectionName(target, collection)) {
        return false;
      }
      if (collection == "array" || collection == "vector") {
        return target.templateArgs.size() == 1;
      }
      if (collection == "map") {
        return target.templateArgs.size() == 2;
      }
      return false;
    }
    return false;
  };

  auto resolveStringTableTarget = [&](const Expr &expr,
                                      const LocalMap &localsIn,
                                      int32_t &stringIndexOut,
                                      size_t &lengthOut) -> bool {
    stringIndexOut = -1;
    lengthOut = 0;
    if (expr.kind == Expr::Kind::StringLiteral) {
      std::string decoded;
      if (!parseStringLiteral(expr.stringValue, decoded)) {
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
    if (!isSimpleCallName(expr, "count") || expr.args.size() != 1) {
      return false;
    }
    const Expr &target = expr.args.front();
    if (target.kind == Expr::Kind::StringLiteral) {
      return true;
    }
    if (target.kind == Expr::Kind::Name) {
      auto it = localsIn.find(target.name);
      return it != localsIn.end() && it->second.valueKind == LocalInfo::ValueKind::String;
    }
    return false;
  };

  auto isFloatTypeName = [](const std::string &name) -> bool {
    return name == "float" || name == "f32" || name == "f64";
  };

  auto isStringTypeName = [](const std::string &name) -> bool {
    return name == "string";
  };

  auto resolveExprPath = [&](const Expr &expr) -> std::string {
    if (!expr.name.empty() && expr.name[0] == '/') {
      return expr.name;
    }
    if (!expr.namespacePrefix.empty()) {
      return expr.namespacePrefix + "/" + expr.name;
    }
    auto importIt = importAliases.find(expr.name);
    if (importIt != importAliases.end()) {
      return importIt->second;
    }
    return "/" + expr.name;
  };

  auto parseMathName = [&](const std::string &name, std::string &out) -> bool {
    if (name.empty()) {
      return false;
    }
    std::string normalized = name;
    if (!normalized.empty() && normalized[0] == '/') {
      normalized.erase(0, 1);
    }
    if (normalized.rfind("math/", 0) == 0) {
      out = normalized.substr(5);
      return true;
    }
    if (normalized.find('/') != std::string::npos) {
      return false;
    }
    if (!hasMathImport) {
      return false;
    }
    out = normalized;
    return true;
  };

  auto getMathBuiltinName = [&](const Expr &expr, std::string &out) -> bool {
    if (expr.kind != Expr::Kind::Call) {
      return false;
    }
    if (!parseMathName(expr.name, out)) {
      return false;
    }
    if (out == "abs" || out == "sign" || out == "min" || out == "max" || out == "clamp" || out == "lerp" ||
        out == "saturate" || out == "floor" || out == "ceil" || out == "round" || out == "trunc" ||
        out == "fract" || out == "sqrt" || out == "cbrt" || out == "pow" || out == "exp" || out == "exp2" ||
        out == "log" || out == "log2" || out == "log10" || out == "sin" || out == "cos" || out == "tan" ||
        out == "asin" || out == "acos" || out == "atan" || out == "atan2" || out == "radians" ||
        out == "degrees" || out == "sinh" || out == "cosh" || out == "tanh" || out == "asinh" ||
        out == "acosh" || out == "atanh" || out == "fma" || out == "hypot" || out == "copysign" ||
        out == "is_nan" || out == "is_inf" || out == "is_finite") {
      return true;
    }
    return false;
  };

  auto getMathConstantName = [&](const std::string &name, std::string &out) -> bool {
    if (!parseMathName(name, out)) {
      return false;
    }
    return out == "pi" || out == "tau" || out == "e";
  };

  auto valueKindFromTypeName = [](const std::string &name) -> LocalInfo::ValueKind {
    if (name == "int" || name == "i32") {
      return LocalInfo::ValueKind::Int32;
    }
    if (name == "i64") {
      return LocalInfo::ValueKind::Int64;
    }
    if (name == "u64") {
      return LocalInfo::ValueKind::UInt64;
    }
    if (name == "bool") {
      return LocalInfo::ValueKind::Bool;
    }
    if (name == "string") {
      return LocalInfo::ValueKind::String;
    }
    return LocalInfo::ValueKind::Unknown;
  };

  auto bindingKind = [](const Expr &expr) -> LocalInfo::Kind {
    for (const auto &transform : expr.transforms) {
      if (transform.name == "Reference") {
        return LocalInfo::Kind::Reference;
      }
      if (transform.name == "Pointer") {
        return LocalInfo::Kind::Pointer;
      }
      if (transform.name == "array") {
        return LocalInfo::Kind::Array;
      }
      if (transform.name == "vector") {
        return LocalInfo::Kind::Vector;
      }
      if (transform.name == "map") {
        return LocalInfo::Kind::Map;
      }
    }
    return LocalInfo::Kind::Value;
  };

  auto isFloatBinding = [&](const Expr &expr) -> bool {
    for (const auto &transform : expr.transforms) {
      if (isBindingQualifierName(transform.name)) {
        continue;
      }
      if (isFloatTypeName(transform.name)) {
        return true;
      }
      if ((transform.name == "Pointer" || transform.name == "Reference") && transform.templateArgs.size() == 1 &&
          isFloatTypeName(transform.templateArgs.front())) {
        return true;
      }
    }
    return false;
  };

  auto isStringBinding = [&](const Expr &expr) -> bool {
    for (const auto &transform : expr.transforms) {
      if (isBindingQualifierName(transform.name)) {
        continue;
      }
      if (isStringTypeName(transform.name)) {
        return true;
      }
      if ((transform.name == "Pointer" || transform.name == "Reference") && transform.templateArgs.size() == 1 &&
          isStringTypeName(transform.templateArgs.front())) {
        return true;
      }
    }
    return false;
  };

  auto bindingValueKind = [&](const Expr &expr, LocalInfo::Kind kind) -> LocalInfo::ValueKind {
    for (const auto &transform : expr.transforms) {
      if (isBindingQualifierName(transform.name)) {
        continue;
      }
      if (transform.name == "Pointer" || transform.name == "Reference") {
        if (transform.templateArgs.size() == 1) {
          return valueKindFromTypeName(transform.templateArgs.front());
        }
        return LocalInfo::ValueKind::Unknown;
      }
      if (transform.name == "array" || transform.name == "vector") {
        if (transform.templateArgs.size() == 1) {
          return valueKindFromTypeName(transform.templateArgs.front());
        }
        return LocalInfo::ValueKind::Unknown;
      }
      if (transform.name == "map") {
        if (transform.templateArgs.size() == 2) {
          return valueKindFromTypeName(transform.templateArgs[1]);
        }
        return LocalInfo::ValueKind::Unknown;
      }
      LocalInfo::ValueKind kindValue = valueKindFromTypeName(transform.name);
      if (kindValue != LocalInfo::ValueKind::Unknown) {
        return kindValue;
      }
    }
    if (kind != LocalInfo::Kind::Value) {
      return LocalInfo::ValueKind::Unknown;
    }
    return LocalInfo::ValueKind::Int32;
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

  auto comparisonKind = [&](LocalInfo::ValueKind left, LocalInfo::ValueKind right) -> LocalInfo::ValueKind {
    if (left == LocalInfo::ValueKind::Bool) {
      left = LocalInfo::ValueKind::Int32;
    }
    if (right == LocalInfo::ValueKind::Bool) {
      right = LocalInfo::ValueKind::Int32;
    }
    return combineNumericKinds(left, right);
  };

  struct ReturnInfo {
    bool returnsVoid = false;
    LocalInfo::ValueKind kind = LocalInfo::ValueKind::Unknown;
  };

  std::unordered_map<std::string, ReturnInfo> returnInfoCache;
  std::unordered_set<std::string> returnInferenceStack;
  std::function<bool(const std::string &, ReturnInfo &)> getReturnInfo;

  std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> inferExprKind;

  auto typeNameForValueKind = [](LocalInfo::ValueKind kind) -> std::string {
    switch (kind) {
      case LocalInfo::ValueKind::Int32:
        return "i32";
      case LocalInfo::ValueKind::Int64:
        return "i64";
      case LocalInfo::ValueKind::UInt64:
        return "u64";
      case LocalInfo::ValueKind::Bool:
        return "bool";
      case LocalInfo::ValueKind::String:
        return "string";
      default:
        return "";
    }
  };
  std::function<const Definition *(const Expr &, const LocalMap &)> resolveMethodCallDefinition;
  resolveMethodCallDefinition = [&](const Expr &callExpr, const LocalMap &localsIn) -> const Definition * {
    if (callExpr.kind != Expr::Kind::Call || callExpr.isBinding || !callExpr.isMethodCall) {
      return nullptr;
    }
    if (callExpr.args.empty()) {
      error = "method call missing receiver";
      return nullptr;
    }
    if (isArrayCountCall(callExpr, localsIn)) {
      return nullptr;
    }
    const Expr &receiver = callExpr.args.front();
    if (isEntryArgsName(receiver, localsIn)) {
      error = "unknown method target for " + callExpr.name;
      return nullptr;
    }
    std::string typeName;
    if (receiver.kind == Expr::Kind::Name) {
      auto it = localsIn.find(receiver.name);
      if (it == localsIn.end()) {
        error = "native backend does not know identifier: " + receiver.name;
        return nullptr;
      }
      if (it->second.kind == LocalInfo::Kind::Array) {
        typeName = "array";
      } else if (it->second.kind == LocalInfo::Kind::Vector) {
        typeName = "vector";
      } else if (it->second.kind == LocalInfo::Kind::Map) {
        typeName = "map";
      } else if (it->second.kind == LocalInfo::Kind::Pointer || it->second.kind == LocalInfo::Kind::Reference) {
        error = "unknown method target for " + callExpr.name;
        return nullptr;
      } else {
        typeName = typeNameForValueKind(it->second.valueKind);
      }
    } else if (receiver.kind == Expr::Kind::Call) {
      std::string collection;
      if (getBuiltinCollectionName(receiver, collection)) {
        if (collection == "array" && receiver.templateArgs.size() == 1) {
          typeName = "array";
        } else if (collection == "vector" && receiver.templateArgs.size() == 1) {
          typeName = "vector";
        } else if (collection == "map" && receiver.templateArgs.size() == 2) {
          typeName = "map";
        }
      }
      if (typeName.empty()) {
        typeName = typeNameForValueKind(inferExprKind(receiver, localsIn));
      }
    } else {
      typeName = typeNameForValueKind(inferExprKind(receiver, localsIn));
    }
    if (typeName.empty()) {
      error = "unknown method target for " + callExpr.name;
      return nullptr;
    }
    const std::string resolved = "/" + typeName + "/" + callExpr.name;
    auto defIt = defMap.find(resolved);
    if (defIt == defMap.end()) {
      error = "unknown method: " + resolved;
      return nullptr;
    }
    return defIt->second;
  };

  std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> inferPointerTargetKind;
  inferPointerTargetKind = [&](const Expr &expr, const LocalMap &localsIn) -> LocalInfo::ValueKind {
    if (expr.kind == Expr::Kind::Name) {
      auto it = localsIn.find(expr.name);
      if (it == localsIn.end()) {
        return LocalInfo::ValueKind::Unknown;
      }
      if (it->second.kind == LocalInfo::Kind::Pointer || it->second.kind == LocalInfo::Kind::Reference) {
        return it->second.valueKind;
      }
      return LocalInfo::ValueKind::Unknown;
    }
    if (expr.kind == Expr::Kind::Call) {
      if (isSimpleCallName(expr, "location") && expr.args.size() == 1) {
        const Expr &target = expr.args.front();
        if (target.kind == Expr::Kind::Name) {
          auto it = localsIn.find(target.name);
          if (it != localsIn.end()) {
            return it->second.valueKind;
          }
        }
        return LocalInfo::ValueKind::Unknown;
      }
      std::string builtin;
      if (getBuiltinOperatorName(expr, builtin) && (builtin == "plus" || builtin == "minus") &&
          expr.args.size() == 2) {
        std::function<bool(const Expr &)> isPointerExpr;
        isPointerExpr = [&](const Expr &candidate) -> bool {
          if (candidate.kind == Expr::Kind::Name) {
            auto it = localsIn.find(candidate.name);
            return it != localsIn.end() && it->second.kind == LocalInfo::Kind::Pointer;
          }
          if (candidate.kind == Expr::Kind::Call && isSimpleCallName(candidate, "location")) {
            return true;
          }
          if (candidate.kind == Expr::Kind::Call) {
            std::string innerName;
            if (getBuiltinOperatorName(candidate, innerName) && (innerName == "plus" || innerName == "minus") &&
                candidate.args.size() == 2) {
              return isPointerExpr(candidate.args[0]) && !isPointerExpr(candidate.args[1]);
            }
          }
          return false;
        };
        if (isPointerExpr(expr.args[0]) && !isPointerExpr(expr.args[1])) {
          return inferPointerTargetKind(expr.args[0], localsIn);
        }
      }
    }
    return LocalInfo::ValueKind::Unknown;
  };

  inferExprKind = [&](const Expr &expr, const LocalMap &localsIn) -> LocalInfo::ValueKind {
    switch (expr.kind) {
      case Expr::Kind::Literal:
        if (expr.isUnsigned) {
          return LocalInfo::ValueKind::UInt64;
        }
        if (expr.intWidth == 64) {
          return LocalInfo::ValueKind::Int64;
        }
        return LocalInfo::ValueKind::Int32;
      case Expr::Kind::BoolLiteral:
        return LocalInfo::ValueKind::Bool;
      case Expr::Kind::StringLiteral:
        return LocalInfo::ValueKind::String;
      case Expr::Kind::Name: {
        auto it = localsIn.find(expr.name);
        if (it == localsIn.end()) {
          return LocalInfo::ValueKind::Unknown;
        }
        if (it->second.kind == LocalInfo::Kind::Value || it->second.kind == LocalInfo::Kind::Reference) {
          return it->second.valueKind;
        }
        return LocalInfo::ValueKind::Unknown;
      }
      case Expr::Kind::Call: {
        if (!expr.isMethodCall) {
          const std::string resolved = resolveExprPath(expr);
          auto defIt = defMap.find(resolved);
          if (defIt != defMap.end()) {
            ReturnInfo info;
            if (getReturnInfo && getReturnInfo(resolved, info) && !info.returnsVoid) {
              return info.kind;
            }
            return LocalInfo::ValueKind::Unknown;
          }
          if (isSimpleCallName(expr, "count") && expr.args.size() == 1 && !isArrayCountCall(expr, localsIn) &&
              !isStringCountCall(expr, localsIn)) {
            Expr methodExpr = expr;
            methodExpr.isMethodCall = true;
            const Definition *callee = resolveMethodCallDefinition(methodExpr, localsIn);
            if (callee) {
              ReturnInfo info;
              if (getReturnInfo && getReturnInfo(callee->fullPath, info) && !info.returnsVoid) {
                return info.kind;
              }
              return LocalInfo::ValueKind::Unknown;
            }
          }
        } else {
          const Definition *callee = resolveMethodCallDefinition(expr, localsIn);
          if (callee) {
            ReturnInfo info;
            if (getReturnInfo && getReturnInfo(callee->fullPath, info) && !info.returnsVoid) {
              return info.kind;
            }
            return LocalInfo::ValueKind::Unknown;
          }
        }
        if (isArrayCountCall(expr, localsIn)) {
          return LocalInfo::ValueKind::Int32;
        }
        if (isStringCountCall(expr, localsIn)) {
          return LocalInfo::ValueKind::Int32;
        }
        std::string accessName;
        if (getBuiltinArrayAccessName(expr, accessName)) {
          if (expr.args.size() != 2) {
            return LocalInfo::ValueKind::Unknown;
          }
          const Expr &target = expr.args.front();
          if (target.kind == Expr::Kind::StringLiteral) {
            return LocalInfo::ValueKind::Int32;
          }
          if (target.kind == Expr::Kind::Name) {
            auto it = localsIn.find(target.name);
            if (it != localsIn.end() && it->second.valueKind == LocalInfo::ValueKind::String) {
              return LocalInfo::ValueKind::Int32;
            }
          }
          if (isEntryArgsName(target, localsIn)) {
            return LocalInfo::ValueKind::Unknown;
          }
          if (target.kind == Expr::Kind::Name) {
            auto it = localsIn.find(target.name);
            if (it != localsIn.end() && it->second.kind == LocalInfo::Kind::Map) {
              if (it->second.mapValueKind != LocalInfo::ValueKind::Unknown &&
                  it->second.mapValueKind != LocalInfo::ValueKind::String) {
                return it->second.mapValueKind;
              }
            }
          } else if (target.kind == Expr::Kind::Call) {
            std::string collection;
            if (getBuiltinCollectionName(target, collection) && collection == "map" && target.templateArgs.size() == 2) {
              LocalInfo::ValueKind kind = valueKindFromTypeName(target.templateArgs[1]);
              if (kind != LocalInfo::ValueKind::Unknown && kind != LocalInfo::ValueKind::String) {
                return kind;
              }
            }
          }
          LocalInfo::ValueKind elemKind = LocalInfo::ValueKind::Unknown;
          if (target.kind == Expr::Kind::Name) {
            auto it = localsIn.find(target.name);
            if (it != localsIn.end() &&
                (it->second.kind == LocalInfo::Kind::Array || it->second.kind == LocalInfo::Kind::Vector)) {
              elemKind = it->second.valueKind;
            }
          } else if (target.kind == Expr::Kind::Call) {
            std::string collection;
            if (getBuiltinCollectionName(target, collection) && (collection == "array" || collection == "vector") &&
                target.templateArgs.size() == 1) {
              elemKind = valueKindFromTypeName(target.templateArgs.front());
            }
          }
          if (elemKind == LocalInfo::ValueKind::Unknown || elemKind == LocalInfo::ValueKind::String) {
            return LocalInfo::ValueKind::Unknown;
          }
          return elemKind;
        }
        std::string builtin;
        if (getBuiltinComparisonName(expr, builtin)) {
          return LocalInfo::ValueKind::Bool;
        }
        if (getBuiltinOperatorName(expr, builtin)) {
          if (builtin == "negate") {
            if (expr.args.size() != 1) {
              return LocalInfo::ValueKind::Unknown;
            }
            return inferExprKind(expr.args.front(), localsIn);
          }
          if (expr.args.size() != 2) {
            return LocalInfo::ValueKind::Unknown;
          }
          auto left = inferExprKind(expr.args[0], localsIn);
          auto right = inferExprKind(expr.args[1], localsIn);
          return combineNumericKinds(left, right);
        }
        if (getBuiltinClampName(expr, hasMathImport)) {
          if (expr.args.size() != 3) {
            return LocalInfo::ValueKind::Unknown;
          }
          auto first = inferExprKind(expr.args[0], localsIn);
          auto second = inferExprKind(expr.args[1], localsIn);
          auto third = inferExprKind(expr.args[2], localsIn);
          return combineNumericKinds(combineNumericKinds(first, second), third);
        }
        if (getBuiltinMinMaxName(expr, builtin, hasMathImport)) {
          if (expr.args.size() != 2) {
            return LocalInfo::ValueKind::Unknown;
          }
          auto left = inferExprKind(expr.args[0], localsIn);
          auto right = inferExprKind(expr.args[1], localsIn);
          return combineNumericKinds(left, right);
        }
        if (getBuiltinLerpName(expr, builtin, hasMathImport)) {
          if (expr.args.size() != 3) {
            return LocalInfo::ValueKind::Unknown;
          }
          auto startKind = inferExprKind(expr.args[0], localsIn);
          auto endKind = inferExprKind(expr.args[1], localsIn);
          auto tKind = inferExprKind(expr.args[2], localsIn);
          return combineNumericKinds(combineNumericKinds(startKind, endKind), tKind);
        }
        if (getBuiltinAbsSignName(expr, builtin, hasMathImport)) {
          if (expr.args.size() != 1) {
            return LocalInfo::ValueKind::Unknown;
          }
          return inferExprKind(expr.args.front(), localsIn);
        }
        if (getBuiltinSaturateName(expr, builtin, hasMathImport)) {
          if (expr.args.size() != 1) {
            return LocalInfo::ValueKind::Unknown;
          }
          return inferExprKind(expr.args.front(), localsIn);
        }
        if (getBuiltinConvertName(expr)) {
          if (expr.templateArgs.size() != 1) {
            return LocalInfo::ValueKind::Unknown;
          }
          const std::string &typeName = expr.templateArgs.front();
          return valueKindFromTypeName(typeName);
        }
        if (isSimpleCallName(expr, "assign")) {
          if (expr.args.size() != 2) {
            return LocalInfo::ValueKind::Unknown;
          }
          const Expr &target = expr.args.front();
          if (target.kind == Expr::Kind::Name) {
            auto it = localsIn.find(target.name);
            if (it != localsIn.end()) {
              if (it->second.kind != LocalInfo::Kind::Value && it->second.kind != LocalInfo::Kind::Reference) {
                return LocalInfo::ValueKind::Unknown;
              }
              return it->second.valueKind;
            }
            return LocalInfo::ValueKind::Unknown;
          }
          if (target.kind == Expr::Kind::Call && isSimpleCallName(target, "dereference") && target.args.size() == 1) {
            return inferPointerTargetKind(target.args.front(), localsIn);
          }
          return LocalInfo::ValueKind::Unknown;
        }
        if (isIfCall(expr) && expr.args.size() == 3) {
          auto isIfBlockEnvelope = [&](const Expr &candidate) -> bool {
            if (candidate.kind != Expr::Kind::Call || candidate.isBinding || candidate.isMethodCall) {
              return false;
            }
            if (!candidate.args.empty() || !candidate.templateArgs.empty()) {
              return false;
            }
            if (!candidate.hasBodyArguments && candidate.bodyArguments.empty()) {
              return false;
            }
            const std::string resolved = resolveExprPath(candidate);
            return defMap.find(resolved) == defMap.end();
          };
          auto inferBranchValueKind = [&](const Expr &candidate, const LocalMap &localsBase) -> LocalInfo::ValueKind {
            if (!isIfBlockEnvelope(candidate)) {
              return inferExprKind(candidate, localsBase);
            }
            LocalMap branchLocals = localsBase;
            bool sawValue = false;
            LocalInfo::ValueKind lastKind = LocalInfo::ValueKind::Unknown;
            for (const auto &bodyExpr : candidate.bodyArguments) {
              if (bodyExpr.isBinding) {
                if (bodyExpr.args.size() != 1) {
                  return LocalInfo::ValueKind::Unknown;
                }
                LocalInfo info;
                info.index = 0;
                info.isMutable = isBindingMutable(bodyExpr);
                info.kind = bindingKind(bodyExpr);
                LocalInfo::ValueKind valueKind = LocalInfo::ValueKind::Unknown;
                if (hasExplicitBindingTypeTransform(bodyExpr)) {
                  valueKind = bindingValueKind(bodyExpr, info.kind);
                } else if (bodyExpr.args.size() == 1 && info.kind == LocalInfo::Kind::Value) {
                  valueKind = inferExprKind(bodyExpr.args.front(), branchLocals);
                  if (valueKind == LocalInfo::ValueKind::Unknown) {
                    valueKind = LocalInfo::ValueKind::Int32;
                  }
                }
                info.valueKind = valueKind;
                branchLocals.emplace(bodyExpr.name, info);
                continue;
              }
              sawValue = true;
              lastKind = inferExprKind(bodyExpr, branchLocals);
            }
            return sawValue ? lastKind : LocalInfo::ValueKind::Unknown;
          };

          LocalInfo::ValueKind thenKind = inferBranchValueKind(expr.args[1], localsIn);
          LocalInfo::ValueKind elseKind = inferBranchValueKind(expr.args[2], localsIn);
          if (thenKind == elseKind) {
            return thenKind;
          }
          return combineNumericKinds(thenKind, elseKind);
        }
        if (isBlockCall(expr) && expr.hasBodyArguments) {
          const std::string resolved = resolveExprPath(expr);
          if (defMap.find(resolved) == defMap.end() && expr.args.empty() && expr.templateArgs.empty() &&
              !hasNamedArguments(expr.argNames)) {
            if (expr.bodyArguments.empty()) {
              return LocalInfo::ValueKind::Unknown;
            }
            LocalMap blockLocals = localsIn;
            LocalInfo::ValueKind result = LocalInfo::ValueKind::Unknown;
            for (const auto &bodyExpr : expr.bodyArguments) {
              if (bodyExpr.isBinding) {
                if (bodyExpr.args.size() != 1) {
                  return LocalInfo::ValueKind::Unknown;
                }
                LocalInfo info;
                info.index = 0;
                info.isMutable = isBindingMutable(bodyExpr);
                info.kind = bindingKind(bodyExpr);
                LocalInfo::ValueKind valueKind = LocalInfo::ValueKind::Unknown;
                if (hasExplicitBindingTypeTransform(bodyExpr)) {
                  valueKind = bindingValueKind(bodyExpr, info.kind);
                } else if (bodyExpr.args.size() == 1 && info.kind == LocalInfo::Kind::Value) {
                  valueKind = inferExprKind(bodyExpr.args.front(), blockLocals);
                  if (valueKind == LocalInfo::ValueKind::Unknown) {
                    valueKind = LocalInfo::ValueKind::Int32;
                  }
                }
                info.valueKind = valueKind;
                blockLocals.emplace(bodyExpr.name, info);
                continue;
              }
              result = inferExprKind(bodyExpr, blockLocals);
            }
            return result;
          }
        }
        if (getBuiltinPointerName(expr, builtin)) {
          if (builtin == "dereference") {
            if (expr.args.size() != 1) {
              return LocalInfo::ValueKind::Unknown;
            }
            return inferPointerTargetKind(expr.args.front(), localsIn);
          }
          return LocalInfo::ValueKind::Unknown;
        }
        return LocalInfo::ValueKind::Unknown;
      }
      default:
        return LocalInfo::ValueKind::Unknown;
    }
  };

  getReturnInfo = [&](const std::string &path, ReturnInfo &outInfo) -> bool {
    auto cached = returnInfoCache.find(path);
    if (cached != returnInfoCache.end()) {
      outInfo = cached->second;
      return true;
    }
    auto defIt = defMap.find(path);
    if (defIt == defMap.end() || !defIt->second) {
      error = "native backend cannot resolve definition: " + path;
      return false;
    }
    if (!returnInferenceStack.insert(path).second) {
      error = "native backend return type inference requires explicit annotation on " + path;
      return false;
    }

    const Definition &def = *defIt->second;
    ReturnInfo info;
