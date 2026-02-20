
  bool hasMathImport = false;
  auto isMathImport = [](const std::string &path) -> bool {
    if (path == "/math/*") {
      return true;
    }
    return path.rfind("/math/", 0) == 0 && path.size() > 6;
  };
  for (const auto &importPath : program.imports) {
    if (isMathImport(importPath)) {
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
    enum class ValueKind { Unknown, Int32, Int64, UInt64, Float32, Float64, Bool, String } valueKind = ValueKind::Unknown;
    ValueKind mapKeyKind = ValueKind::Unknown;
    ValueKind mapValueKind = ValueKind::Unknown;
    enum class StringSource { None, TableIndex, ArgvIndex } stringSource = StringSource::None;
    int32_t stringIndex = -1;
    bool argvChecked = true;
    bool referenceToArray = false;
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

  auto emitVectorIndexOutOfBounds = [&]() {
    uint64_t flags = encodePrintFlags(true, true);
    int32_t msgIndex = internString("vector index out of bounds");
    function.instructions.push_back(
        {IrOpcode::PrintString, encodePrintStringImm(static_cast<uint64_t>(msgIndex), flags)});
    function.instructions.push_back({IrOpcode::PushI32, 3});
    function.instructions.push_back({IrOpcode::ReturnI32, 0});
  };

  auto emitVectorPopOnEmpty = [&]() {
    uint64_t flags = encodePrintFlags(true, true);
    int32_t msgIndex = internString("vector pop on empty");
    function.instructions.push_back(
        {IrOpcode::PrintString, encodePrintStringImm(static_cast<uint64_t>(msgIndex), flags)});
    function.instructions.push_back({IrOpcode::PushI32, 3});
    function.instructions.push_back({IrOpcode::ReturnI32, 0});
  };

  auto emitVectorCapacityExceeded = [&]() {
    uint64_t flags = encodePrintFlags(true, true);
    int32_t msgIndex = internString("vector capacity exceeded");
    function.instructions.push_back(
        {IrOpcode::PrintString, encodePrintStringImm(static_cast<uint64_t>(msgIndex), flags)});
    function.instructions.push_back({IrOpcode::PushI32, 3});
    function.instructions.push_back({IrOpcode::ReturnI32, 0});
  };

  auto emitVectorReserveNegative = [&]() {
    uint64_t flags = encodePrintFlags(true, true);
    int32_t msgIndex = internString("vector reserve expects non-negative capacity");
    function.instructions.push_back(
        {IrOpcode::PrintString, encodePrintStringImm(static_cast<uint64_t>(msgIndex), flags)});
    function.instructions.push_back({IrOpcode::PushI32, 3});
    function.instructions.push_back({IrOpcode::ReturnI32, 0});
  };

  auto emitVectorReserveExceeded = [&]() {
    uint64_t flags = encodePrintFlags(true, true);
    int32_t msgIndex = internString("vector reserve exceeds capacity");
    function.instructions.push_back(
        {IrOpcode::PrintString, encodePrintStringImm(static_cast<uint64_t>(msgIndex), flags)});
    function.instructions.push_back({IrOpcode::PushI32, 3});
    function.instructions.push_back({IrOpcode::ReturnI32, 0});
  };

  auto emitLoopCountNegative = [&]() {
    uint64_t flags = encodePrintFlags(true, true);
    int32_t msgIndex = internString("loop count must be non-negative");
    function.instructions.push_back(
        {IrOpcode::PrintString, encodePrintStringImm(static_cast<uint64_t>(msgIndex), flags)});
    function.instructions.push_back({IrOpcode::PushI32, 3});
    function.instructions.push_back({IrOpcode::ReturnI32, 0});
  };

  auto emitPowNegativeExponent = [&]() {
    uint64_t flags = encodePrintFlags(true, true);
    int32_t msgIndex = internString("pow exponent must be non-negative");
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
      if (it == localsIn.end()) {
        return false;
      }
      if (it->second.kind == LocalInfo::Kind::Reference) {
        return it->second.referenceToArray;
      }
      return it->second.kind == LocalInfo::Kind::Array || it->second.kind == LocalInfo::Kind::Vector ||
             it->second.kind == LocalInfo::Kind::Map;
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
  auto isVectorCapacityCall = [&](const Expr &expr, const LocalMap &localsIn) -> bool {
    if (!isSimpleCallName(expr, "capacity") || expr.args.size() != 1) {
      return false;
    }
    const Expr &target = expr.args.front();
    if (target.kind == Expr::Kind::Name) {
      auto it = localsIn.find(target.name);
      return it != localsIn.end() && it->second.kind == LocalInfo::Kind::Vector;
    }
    if (target.kind == Expr::Kind::Call) {
      std::string collection;
      if (!getBuiltinCollectionName(target, collection)) {
        return false;
      }
      return collection == "vector" && target.templateArgs.size() == 1;
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

  auto isStringTypeName = [](const std::string &name) -> bool {
    return name == "string";
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
    if (name == "float" || name == "f32") {
      return LocalInfo::ValueKind::Float32;
    }
    if (name == "f64") {
      return LocalInfo::ValueKind::Float64;
    }
    if (name == "bool") {
      return LocalInfo::ValueKind::Bool;
    }
    if (name == "string") {
      return LocalInfo::ValueKind::String;
    }
    return LocalInfo::ValueKind::Unknown;
  };
  auto trimText = [](const std::string &text) {
    size_t start = 0;
    while (start < text.size() && std::isspace(static_cast<unsigned char>(text[start]))) {
      ++start;
    }
    size_t end = text.size();
    while (end > start && std::isspace(static_cast<unsigned char>(text[end - 1]))) {
      --end;
    }
    return text.substr(start, end - start);
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
        info.valueKind = valueKindFromTypeName(trimText(arg));
      }
      return;
    }
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
