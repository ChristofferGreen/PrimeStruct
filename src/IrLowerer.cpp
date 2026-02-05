#include "primec/IrLowerer.h"
#include "primec/StringLiteral.h"

#include <functional>
#include <limits>
#include <unordered_map>
#include <unordered_set>

namespace primec {
namespace {

bool isSimpleCallName(const Expr &expr, const char *nameToMatch) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  std::string name = expr.name;
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  if (name.find('/') != std::string::npos) {
    return false;
  }
  return name == nameToMatch;
}

bool isReturnCall(const Expr &expr) {
  return isSimpleCallName(expr, "return");
}

bool isIfCall(const Expr &expr) {
  return isSimpleCallName(expr, "if");
}

bool isThenCall(const Expr &expr) {
  return isSimpleCallName(expr, "then");
}

bool isElseCall(const Expr &expr) {
  return isSimpleCallName(expr, "else");
}

bool isRepeatCall(const Expr &expr) {
  return isSimpleCallName(expr, "repeat");
}

enum class PrintTarget { Out, Err };

struct PrintBuiltin {
  PrintTarget target = PrintTarget::Out;
  bool newline = false;
  std::string name;
};

bool getPrintBuiltin(const Expr &expr, PrintBuiltin &out) {
  if (isSimpleCallName(expr, "print")) {
    out.target = PrintTarget::Out;
    out.newline = false;
    out.name = "print";
    return true;
  }
  if (isSimpleCallName(expr, "print_line")) {
    out.target = PrintTarget::Out;
    out.newline = true;
    out.name = "print_line";
    return true;
  }
  if (isSimpleCallName(expr, "print_error")) {
    out.target = PrintTarget::Err;
    out.newline = false;
    out.name = "print_error";
    return true;
  }
  if (isSimpleCallName(expr, "print_line_error")) {
    out.target = PrintTarget::Err;
    out.newline = true;
    out.name = "print_line_error";
    return true;
  }
  return false;
}

bool getBuiltinOperatorName(const Expr &expr, std::string &out) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  std::string name = expr.name;
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  if (name.find('/') != std::string::npos) {
    return false;
  }
  if (name == "plus" || name == "minus" || name == "multiply" || name == "divide" || name == "negate") {
    out = name;
    return true;
  }
  return false;
}

bool getBuiltinComparisonName(const Expr &expr, std::string &out) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  std::string name = expr.name;
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  if (name.find('/') != std::string::npos) {
    return false;
  }
  if (name == "greater_than" || name == "less_than" || name == "equal" || name == "not_equal" ||
      name == "greater_equal" || name == "less_equal" || name == "and" || name == "or" || name == "not") {
    out = name;
    return true;
  }
  return false;
}

bool getBuiltinClampName(const Expr &expr) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  std::string name = expr.name;
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  if (name.find('/') != std::string::npos) {
    return false;
  }
  return name == "clamp";
}

bool getBuiltinConvertName(const Expr &expr) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  std::string name = expr.name;
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  if (name.find('/') != std::string::npos) {
    return false;
  }
  return name == "convert";
}

bool getBuiltinArrayAccessName(const Expr &expr, std::string &out) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  std::string name = expr.name;
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  if (name.find('/') != std::string::npos) {
    return false;
  }
  if (name == "at" || name == "at_unsafe") {
    out = name;
    return true;
  }
  return false;
}

bool getBuiltinPointerName(const Expr &expr, std::string &out) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  std::string name = expr.name;
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  if (name.find('/') != std::string::npos) {
    return false;
  }
  if (name == "dereference" || name == "location") {
    out = name;
    return true;
  }
  return false;
}

bool getBuiltinCollectionName(const Expr &expr, std::string &out) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  std::string name = expr.name;
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  if (name.find('/') != std::string::npos) {
    return false;
  }
  if (name == "array" || name == "map") {
    out = name;
    return true;
  }
  return false;
}

} // namespace

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

  std::unordered_map<std::string, const Definition *> defMap;
  defMap.reserve(program.definitions.size());
  for (const auto &def : program.definitions) {
    defMap.emplace(def.fullPath, &def);
  }

  bool hasReturnTransform = false;
  bool returnsVoid = false;
  for (const auto &transform : entryDef->transforms) {
    if (transform.name != "return") {
      continue;
    }
    hasReturnTransform = true;
    if (transform.templateArg && *transform.templateArg == "void") {
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
    enum class Kind { Value, Pointer, Reference, Array } kind = Kind::Value;
    enum class ValueKind { Unknown, Int32, Int64, UInt64, Bool, String } valueKind = ValueKind::Unknown;
    enum class StringSource { None, TableIndex, ArgvIndex } stringSource = StringSource::None;
    int32_t stringIndex = -1;
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
      if (transform.templateArg) {
        templateArg = *transform.templateArg;
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
  auto isEntryArgsCountCall = [&](const Expr &expr, const LocalMap &localsIn) -> bool {
    if (!isSimpleCallName(expr, "count")) {
      return false;
    }
    if (expr.args.size() != 1) {
      return false;
    }
    return isEntryArgsName(expr.args.front(), localsIn);
  };

  auto isFloatTypeName = [](const std::string &name) -> bool {
    return name == "float" || name == "f32" || name == "f64";
  };

  auto isStringTypeName = [](const std::string &name) -> bool {
    return name == "string";
  };

  auto resolveExprPath = [](const Expr &expr) -> std::string {
    if (!expr.name.empty() && expr.name[0] == '/') {
      return expr.name;
    }
    if (!expr.namespacePrefix.empty()) {
      return expr.namespacePrefix + "/" + expr.name;
    }
    return "/" + expr.name;
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
      if ((transform.name == "Pointer" || transform.name == "Reference") && transform.templateArg &&
          isFloatTypeName(*transform.templateArg)) {
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
      if ((transform.name == "Pointer" || transform.name == "Reference") && transform.templateArg &&
          isStringTypeName(*transform.templateArg)) {
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
        if (transform.templateArg) {
          return valueKindFromTypeName(*transform.templateArg);
        }
        return LocalInfo::ValueKind::Unknown;
      }
      if (transform.name == "array") {
        if (transform.templateArg) {
          return valueKindFromTypeName(*transform.templateArg);
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
    return LocalInfo::ValueKind::Unknown;
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

  std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> inferExprKind;
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
      case Expr::Kind::Name: {
        auto it = localsIn.find(expr.name);
        if (it == localsIn.end()) {
          return LocalInfo::ValueKind::Unknown;
        }
        if (it->second.kind == LocalInfo::Kind::Value || it->second.kind == LocalInfo::Kind::Reference) {
          if (it->second.valueKind == LocalInfo::ValueKind::String) {
            return LocalInfo::ValueKind::Unknown;
          }
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
        }
        if (isEntryArgsCountCall(expr, localsIn)) {
          return LocalInfo::ValueKind::Int32;
        }
        if (isSimpleCallName(expr, "count") && expr.args.size() == 1) {
          if (isEntryArgsName(expr.args.front(), localsIn)) {
            return LocalInfo::ValueKind::Int32;
          }
          const Expr &target = expr.args.front();
          if (target.kind == Expr::Kind::Name) {
            auto it = localsIn.find(target.name);
            if (it != localsIn.end() && it->second.kind == LocalInfo::Kind::Array) {
              return LocalInfo::ValueKind::Int32;
            }
          }
          if (target.kind == Expr::Kind::Call) {
            std::string collection;
            if (getBuiltinCollectionName(target, collection) && collection == "array" && target.templateArgs.size() == 1) {
              return LocalInfo::ValueKind::Int32;
            }
          }
        }
        std::string accessName;
        if (getBuiltinArrayAccessName(expr, accessName)) {
          if (expr.args.size() != 2) {
            return LocalInfo::ValueKind::Unknown;
          }
          const Expr &target = expr.args.front();
          if (isEntryArgsName(target, localsIn)) {
            return LocalInfo::ValueKind::Unknown;
          }
          LocalInfo::ValueKind elemKind = LocalInfo::ValueKind::Unknown;
          if (target.kind == Expr::Kind::Name) {
            auto it = localsIn.find(target.name);
            if (it != localsIn.end() && it->second.kind == LocalInfo::Kind::Array) {
              elemKind = it->second.valueKind;
            }
          } else if (target.kind == Expr::Kind::Call) {
            std::string collection;
            if (getBuiltinCollectionName(target, collection) && collection == "array" && target.templateArgs.size() == 1) {
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
        if (getBuiltinClampName(expr)) {
          if (expr.args.size() != 3) {
            return LocalInfo::ValueKind::Unknown;
          }
          auto first = inferExprKind(expr.args[0], localsIn);
          auto second = inferExprKind(expr.args[1], localsIn);
          auto third = inferExprKind(expr.args[2], localsIn);
          return combineNumericKinds(combineNumericKinds(first, second), third);
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
    bool hasReturnTransformLocal = false;
    for (const auto &transform : def.transforms) {
      if (transform.name == "struct" || transform.name == "pod" || transform.name == "stack" || transform.name == "heap" ||
          transform.name == "buffer") {
        info.returnsVoid = true;
        hasReturnTransformLocal = true;
        break;
      }
      if (transform.name != "return") {
        continue;
      }
      hasReturnTransformLocal = true;
      if (!transform.templateArg) {
        continue;
      }
      if (*transform.templateArg == "void") {
        info.returnsVoid = true;
        break;
      }
      info.kind = valueKindFromTypeName(*transform.templateArg);
      info.returnsVoid = false;
      break;
    }

    if (hasReturnTransformLocal) {
      if (!info.returnsVoid) {
        if (info.kind == LocalInfo::ValueKind::Unknown) {
          error = "native backend does not support return type on " + def.fullPath;
          returnInferenceStack.erase(path);
          return false;
        }
        if (info.kind == LocalInfo::ValueKind::String) {
          error = "native backend does not support string return types on " + def.fullPath;
          returnInferenceStack.erase(path);
          return false;
        }
      }
    } else {
      if (!def.hasReturnStatement) {
        info.returnsVoid = true;
      } else {
        LocalMap localsForInference;
        for (const auto &param : def.parameters) {
          if (isFloatBinding(param)) {
            error = "native backend does not support float types";
            returnInferenceStack.erase(path);
            return false;
          }
          LocalInfo paramInfo;
          paramInfo.index = 0;
          paramInfo.isMutable = isBindingMutable(param);
          paramInfo.kind = bindingKind(param);
          paramInfo.valueKind = bindingValueKind(param, paramInfo.kind);
          if (isStringBinding(param)) {
            if (paramInfo.kind != LocalInfo::Kind::Value) {
              error = "native backend does not support string pointers or references";
              returnInferenceStack.erase(path);
              return false;
            }
          }
          if (paramInfo.valueKind == LocalInfo::ValueKind::Unknown) {
            error = "native backend requires typed parameters on " + def.fullPath;
            returnInferenceStack.erase(path);
            return false;
          }
          localsForInference.emplace(param.name, paramInfo);
        }

        LocalInfo::ValueKind inferred = LocalInfo::ValueKind::Unknown;
        bool sawReturnLocal = false;
        bool inferredVoid = false;
        std::function<bool(const Expr &, LocalMap &)> inferStatement;
        inferStatement = [&](const Expr &stmt, LocalMap &activeLocals) -> bool {
          if (stmt.isBinding) {
            if (isFloatBinding(stmt)) {
              error = "native backend does not support float types";
              return false;
            }
            LocalInfo bindingInfo;
            bindingInfo.index = 0;
            bindingInfo.isMutable = isBindingMutable(stmt);
            bindingInfo.kind = bindingKind(stmt);
            bindingInfo.valueKind = bindingValueKind(stmt, bindingInfo.kind);
            if (isStringBinding(stmt) && bindingInfo.kind != LocalInfo::Kind::Value) {
              error = "native backend does not support string pointers or references";
              return false;
            }
            if (bindingInfo.valueKind == LocalInfo::ValueKind::Unknown) {
              error = "native backend requires typed bindings on " + def.fullPath;
              return false;
            }
            activeLocals.emplace(stmt.name, bindingInfo);
            return true;
          }
          if (isReturnCall(stmt)) {
            sawReturnLocal = true;
            if (stmt.args.empty()) {
              inferredVoid = true;
              return true;
            }
            LocalInfo::ValueKind kind = inferExprKind(stmt.args.front(), activeLocals);
            if (kind == LocalInfo::ValueKind::Unknown) {
              error = "unable to infer return type on " + def.fullPath;
              return false;
            }
            if (kind == LocalInfo::ValueKind::String) {
              error = "native backend does not support string return types on " + def.fullPath;
              return false;
            }
            if (inferred == LocalInfo::ValueKind::Unknown) {
              inferred = kind;
              return true;
            }
            if (inferred != kind) {
              error = "conflicting return types on " + def.fullPath;
              return false;
            }
            return true;
          }
          if (isIfCall(stmt) && stmt.args.size() == 3) {
            const Expr &thenBlock = stmt.args[1];
            const Expr &elseBlock = stmt.args[2];
            auto walkBlock = [&](const Expr &block) -> bool {
              LocalMap blockLocals = activeLocals;
              for (const auto &bodyStmt : block.bodyArguments) {
                if (!inferStatement(bodyStmt, blockLocals)) {
                  return false;
                }
              }
              return true;
            };
            if (!walkBlock(thenBlock)) {
              return false;
            }
            if (!walkBlock(elseBlock)) {
              return false;
            }
          }
          if (isRepeatCall(stmt)) {
            LocalMap blockLocals = activeLocals;
            for (const auto &bodyStmt : stmt.bodyArguments) {
              if (!inferStatement(bodyStmt, blockLocals)) {
                return false;
              }
            }
          }
          return true;
        };

        LocalMap locals = localsForInference;
        for (const auto &stmt : def.statements) {
          if (!inferStatement(stmt, locals)) {
            returnInferenceStack.erase(path);
            return false;
          }
        }
        if (!sawReturnLocal || inferredVoid) {
          if (sawReturnLocal && inferred != LocalInfo::ValueKind::Unknown) {
            error = "conflicting return types on " + def.fullPath;
            returnInferenceStack.erase(path);
            return false;
          }
          info.returnsVoid = true;
        } else {
          info.returnsVoid = false;
          info.kind = inferred;
          if (info.kind == LocalInfo::ValueKind::Unknown) {
            error = "unable to infer return type on " + def.fullPath;
            returnInferenceStack.erase(path);
            return false;
          }
        }
      }
    }

    returnInferenceStack.erase(path);
    returnInfoCache.emplace(path, info);
    outInfo = info;
    return true;
  };

  struct InlineContext {
    std::string defPath;
    bool returnsVoid = false;
    LocalInfo::ValueKind returnKind = LocalInfo::ValueKind::Unknown;
    int32_t returnLocal = -1;
    std::vector<size_t> returnJumps;
  };

  InlineContext *activeInlineContext = nullptr;
  std::unordered_set<std::string> inlineStack;

  std::function<bool(const Expr &, const LocalMap &)> emitExpr;
  std::function<bool(const Expr &, LocalMap &)> emitStatement;
  auto allocTempLocal = [&]() -> int32_t {
    return nextLocal++;
  };

  auto emitBlock = [&](const Expr &blockExpr, LocalMap &blockLocals) -> bool {
    if (blockExpr.kind != Expr::Kind::Call) {
      error = "native backend expects then/else blocks to be calls";
      return false;
    }
    if (!blockExpr.args.empty()) {
      error = "native backend does not support arguments on then/else blocks";
      return false;
    }
    for (const auto &stmt : blockExpr.bodyArguments) {
      if (!emitStatement(stmt, blockLocals)) {
        return false;
      }
    }
    return true;
  };

  auto emitCompareToZero = [&](LocalInfo::ValueKind kind, bool equals) -> bool {
    if (kind == LocalInfo::ValueKind::Int64 || kind == LocalInfo::ValueKind::UInt64) {
      function.instructions.push_back({IrOpcode::PushI64, 0});
      function.instructions.push_back({equals ? IrOpcode::CmpEqI64 : IrOpcode::CmpNeI64, 0});
      return true;
    }
    if (kind == LocalInfo::ValueKind::Int32 || kind == LocalInfo::ValueKind::Bool) {
      function.instructions.push_back({IrOpcode::PushI32, 0});
      function.instructions.push_back({equals ? IrOpcode::CmpEqI32 : IrOpcode::CmpNeI32, 0});
      return true;
    }
    error = "boolean conversion requires numeric operand";
    return false;
  };

  auto resolveDefinitionCall = [&](const Expr &callExpr) -> const Definition * {
    if (callExpr.kind != Expr::Kind::Call || callExpr.isBinding || callExpr.isMethodCall) {
      return nullptr;
    }
    const std::string resolved = resolveExprPath(callExpr);
    auto it = defMap.find(resolved);
    if (it == defMap.end()) {
      return nullptr;
    }
    return it->second;
  };

  auto buildOrderedCallArguments = [&](const Expr &callExpr,
                                       const Definition &callee,
                                       std::vector<const Expr *> &ordered) -> bool {
    ordered.assign(callee.parameters.size(), nullptr);
    size_t positionalIndex = 0;
    for (size_t i = 0; i < callExpr.args.size(); ++i) {
      if (i < callExpr.argNames.size() && callExpr.argNames[i].has_value()) {
        const std::string &name = *callExpr.argNames[i];
        size_t index = callee.parameters.size();
        for (size_t p = 0; p < callee.parameters.size(); ++p) {
          if (callee.parameters[p].name == name) {
            index = p;
            break;
          }
        }
        if (index >= callee.parameters.size()) {
          error = "unknown named argument: " + name;
          return false;
        }
        if (ordered[index] != nullptr) {
          error = "named argument duplicates parameter: " + name;
          return false;
        }
        ordered[index] = &callExpr.args[i];
        continue;
      }
      while (positionalIndex < ordered.size() && ordered[positionalIndex] != nullptr) {
        ++positionalIndex;
      }
      if (positionalIndex >= ordered.size()) {
        error = "argument count mismatch";
        return false;
      }
      ordered[positionalIndex] = &callExpr.args[i];
      ++positionalIndex;
    }
    for (size_t i = 0; i < ordered.size(); ++i) {
      if (ordered[i] != nullptr) {
        continue;
      }
      if (!callee.parameters[i].args.empty()) {
        ordered[i] = &callee.parameters[i].args.front();
        continue;
      }
      error = "argument count mismatch";
      return false;
    }
    return true;
  };

  auto emitStringValueForCall = [&](const Expr &arg,
                                    const LocalMap &callerLocals,
                                    LocalInfo::StringSource &sourceOut,
                                    int32_t &stringIndexOut) -> bool {
    sourceOut = LocalInfo::StringSource::None;
    stringIndexOut = -1;
    if (arg.kind == Expr::Kind::StringLiteral) {
      std::string decoded;
      if (!parseStringLiteral(arg.stringValue, decoded)) {
        return false;
      }
      int32_t index = internString(decoded);
      function.instructions.push_back({IrOpcode::PushI64, static_cast<uint64_t>(index)});
      sourceOut = LocalInfo::StringSource::TableIndex;
      stringIndexOut = index;
      return true;
    }
    if (arg.kind == Expr::Kind::Name) {
      auto it = callerLocals.find(arg.name);
      if (it == callerLocals.end()) {
        error = "native backend does not know identifier: " + arg.name;
        return false;
      }
      if (it->second.valueKind != LocalInfo::ValueKind::String || it->second.stringSource == LocalInfo::StringSource::None) {
        error = "native backend requires string arguments to use string literals, bindings, or entry args";
        return false;
      }
      sourceOut = it->second.stringSource;
      stringIndexOut = it->second.stringIndex;
      function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index)});
      return true;
    }
    if (arg.kind == Expr::Kind::Call) {
      std::string accessName;
      if (!getBuiltinArrayAccessName(arg, accessName)) {
        error = "native backend requires string arguments to use string literals, bindings, or entry args";
        return false;
      }
      if (arg.args.size() != 2) {
        error = accessName + " requires exactly two arguments";
        return false;
      }
      if (!isEntryArgsName(arg.args.front(), callerLocals)) {
        error = "native backend only supports entry argument indexing";
        return false;
      }
      LocalInfo::ValueKind indexKind = inferExprKind(arg.args[1], callerLocals);
      if (indexKind == LocalInfo::ValueKind::Bool) {
        indexKind = LocalInfo::ValueKind::Int32;
      }
      if (indexKind != LocalInfo::ValueKind::Int32) {
        error = "native backend only supports i32 indices for " + accessName;
        return false;
      }

      const int32_t argvIndexLocal = allocTempLocal();
      if (!emitExpr(arg.args[1], callerLocals)) {
        return false;
      }
      function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(argvIndexLocal)});

      if (accessName == "at") {
        function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(argvIndexLocal)});
        function.instructions.push_back({IrOpcode::PushI32, 0});
        function.instructions.push_back({IrOpcode::CmpLtI32, 0});
        size_t jumpNonNegative = function.instructions.size();
        function.instructions.push_back({IrOpcode::JumpIfZero, 0});
        emitArrayIndexOutOfBounds();
        size_t nonNegativeIndex = function.instructions.size();
        function.instructions[jumpNonNegative].imm = static_cast<int32_t>(nonNegativeIndex);

        function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(argvIndexLocal)});
        function.instructions.push_back({IrOpcode::PushArgc, 0});
        function.instructions.push_back({IrOpcode::CmpGeI32, 0});
        size_t jumpInRange = function.instructions.size();
        function.instructions.push_back({IrOpcode::JumpIfZero, 0});
        emitArrayIndexOutOfBounds();
        size_t inRangeIndex = function.instructions.size();
        function.instructions[jumpInRange].imm = static_cast<int32_t>(inRangeIndex);
      }

      function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(argvIndexLocal)});
      sourceOut = LocalInfo::StringSource::ArgvIndex;
      return true;
    }
    error = "native backend requires string arguments to use string literals, bindings, or entry args";
    return false;
  };

  auto emitInlineDefinitionCall = [&](const Expr &callExpr,
                                      const Definition &callee,
                                      const LocalMap &callerLocals,
                                      bool requireValue) -> bool {
    ReturnInfo returnInfo;
    if (!getReturnInfo(callee.fullPath, returnInfo)) {
      return false;
    }
    if (returnInfo.returnsVoid && requireValue) {
      error = "void call not allowed in expression context: " + callee.fullPath;
      return false;
    }
    if (!inlineStack.insert(callee.fullPath).second) {
      error = "native backend does not support recursive calls: " + callee.fullPath;
      return false;
    }

    std::vector<const Expr *> orderedArgs;
    if (!buildOrderedCallArguments(callExpr, callee, orderedArgs)) {
      inlineStack.erase(callee.fullPath);
      return false;
    }

    LocalMap calleeLocals;
    for (size_t i = 0; i < callee.parameters.size(); ++i) {
      const Expr &param = callee.parameters[i];
      if (isFloatBinding(param)) {
        error = "native backend does not support float types";
        inlineStack.erase(callee.fullPath);
        return false;
      }
      LocalInfo paramInfo;
      paramInfo.index = nextLocal++;
      paramInfo.isMutable = isBindingMutable(param);
      paramInfo.kind = bindingKind(param);
      paramInfo.valueKind = bindingValueKind(param, paramInfo.kind);

      if (isStringBinding(param)) {
        if (paramInfo.kind != LocalInfo::Kind::Value) {
          error = "native backend does not support string pointers or references";
          inlineStack.erase(callee.fullPath);
          return false;
        }
        if (!orderedArgs[i]) {
          error = "argument count mismatch";
          inlineStack.erase(callee.fullPath);
          return false;
        }
        LocalInfo::StringSource source = LocalInfo::StringSource::None;
        int32_t index = -1;
        if (!emitStringValueForCall(*orderedArgs[i], callerLocals, source, index)) {
          inlineStack.erase(callee.fullPath);
          return false;
        }
        paramInfo.valueKind = LocalInfo::ValueKind::String;
        paramInfo.stringSource = source;
        paramInfo.stringIndex = index;
        calleeLocals.emplace(param.name, paramInfo);
        function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(paramInfo.index)});
        continue;
      }

      if (paramInfo.valueKind == LocalInfo::ValueKind::Unknown || paramInfo.valueKind == LocalInfo::ValueKind::String) {
        error = "native backend only supports numeric/bool or string parameters";
        inlineStack.erase(callee.fullPath);
        return false;
      }

      if (!orderedArgs[i]) {
        error = "argument count mismatch";
        inlineStack.erase(callee.fullPath);
        return false;
      }
      if (!emitExpr(*orderedArgs[i], callerLocals)) {
        inlineStack.erase(callee.fullPath);
        return false;
      }
      calleeLocals.emplace(param.name, paramInfo);
      function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(paramInfo.index)});
    }

    InlineContext context;
    context.defPath = callee.fullPath;
    context.returnsVoid = returnInfo.returnsVoid;
    context.returnKind = returnInfo.kind;
    if (!context.returnsVoid) {
      context.returnLocal = allocTempLocal();
      const IrOpcode zeroOp =
          (context.returnKind == LocalInfo::ValueKind::Int64 || context.returnKind == LocalInfo::ValueKind::UInt64)
              ? IrOpcode::PushI64
              : IrOpcode::PushI32;
      function.instructions.push_back({zeroOp, 0});
      function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(context.returnLocal)});
    }

    InlineContext *prevContext = activeInlineContext;
    activeInlineContext = &context;
    for (const auto &stmt : callee.statements) {
      if (!emitStatement(stmt, calleeLocals)) {
        activeInlineContext = prevContext;
        inlineStack.erase(callee.fullPath);
        return false;
      }
    }
    size_t endIndex = function.instructions.size();
    for (size_t jumpIndex : context.returnJumps) {
      function.instructions[jumpIndex].imm = static_cast<int32_t>(endIndex);
    }
    activeInlineContext = prevContext;

    if (!context.returnsVoid) {
      function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(context.returnLocal)});
    }

    inlineStack.erase(callee.fullPath);
    return true;
  };

  emitExpr = [&](const Expr &expr, const LocalMap &localsIn) -> bool {
    switch (expr.kind) {
      case Expr::Kind::Literal: {
        if (expr.intWidth == 64 || expr.isUnsigned) {
          IrInstruction inst;
          inst.op = IrOpcode::PushI64;
          inst.imm = expr.literalValue;
          function.instructions.push_back(inst);
          return true;
        }
        int64_t value = static_cast<int64_t>(expr.literalValue);
        if (value < std::numeric_limits<int32_t>::min() || value > std::numeric_limits<int32_t>::max()) {
          error = "i32 literal out of range for native backend";
          return false;
        }
        IrInstruction inst;
        inst.op = IrOpcode::PushI32;
        inst.imm = static_cast<int32_t>(value);
        function.instructions.push_back(inst);
        return true;
      }
      case Expr::Kind::FloatLiteral:
        error = "native backend does not support float literals";
        return false;
      case Expr::Kind::StringLiteral:
        error = "native backend does not support string literals";
        return false;
      case Expr::Kind::BoolLiteral: {
        IrInstruction inst;
        inst.op = IrOpcode::PushI32;
        inst.imm = expr.boolValue ? 1 : 0;
        function.instructions.push_back(inst);
        return true;
      }
      case Expr::Kind::Name: {
        auto it = localsIn.find(expr.name);
        if (it != localsIn.end()) {
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index)});
          if (it->second.kind == LocalInfo::Kind::Reference) {
            function.instructions.push_back({IrOpcode::LoadIndirect, 0});
          }
          return true;
        }
        if (hasEntryArgs && expr.name == entryArgsName) {
          error = "native backend only supports count() on entry arguments";
          return false;
        }
        error = "native backend does not know identifier: " + expr.name;
        return false;
      }
      case Expr::Kind::Call: {
        if (const Definition *callee = resolveDefinitionCall(expr)) {
          if (!expr.bodyArguments.empty()) {
            error = "native backend does not support block arguments on calls";
            return false;
          }
          return emitInlineDefinitionCall(expr, *callee, localsIn, true);
        }
        if (isSimpleCallName(expr, "count")) {
          if (expr.args.size() != 1) {
            error = "count requires exactly one argument";
            return false;
          }
          if (isEntryArgsName(expr.args.front(), localsIn)) {
            function.instructions.push_back({IrOpcode::PushArgc, 0});
            return true;
          }
          if (!emitExpr(expr.args.front(), localsIn)) {
            return false;
          }
          function.instructions.push_back({IrOpcode::LoadIndirect, 0});
          return true;
        }
        PrintBuiltin printBuiltin;
        if (getPrintBuiltin(expr, printBuiltin)) {
          error = printBuiltin.name + " is only supported as a statement in the native backend";
          return false;
        }
        std::string accessName;
        if (getBuiltinArrayAccessName(expr, accessName)) {
          if (expr.args.size() != 2) {
            error = accessName + " requires exactly two arguments";
            return false;
          }
          if (isEntryArgsName(expr.args.front(), localsIn)) {
            error = "native backend only supports entry argument indexing in print calls or string bindings";
            return false;
          }

          LocalInfo::ValueKind elemKind = LocalInfo::ValueKind::Unknown;
          const Expr &target = expr.args.front();
          if (target.kind == Expr::Kind::Name) {
            auto it = localsIn.find(target.name);
            if (it != localsIn.end() && it->second.kind == LocalInfo::Kind::Array) {
              elemKind = it->second.valueKind;
            }
          } else if (target.kind == Expr::Kind::Call) {
            std::string collection;
            if (getBuiltinCollectionName(target, collection) && collection == "array" && target.templateArgs.size() == 1) {
              elemKind = valueKindFromTypeName(target.templateArgs.front());
            }
          }
          if (elemKind == LocalInfo::ValueKind::Unknown || elemKind == LocalInfo::ValueKind::String) {
            error = "native backend only supports at() on numeric/bool arrays";
            return false;
          }

          LocalInfo::ValueKind indexKind = inferExprKind(expr.args[1], localsIn);
          if (indexKind == LocalInfo::ValueKind::Bool) {
            indexKind = LocalInfo::ValueKind::Int32;
          }
          if (indexKind != LocalInfo::ValueKind::Int32) {
            error = "native backend only supports i32 indices for " + accessName;
            return false;
          }

          const int32_t ptrLocal = allocTempLocal();
          if (!emitExpr(expr.args[0], localsIn)) {
            return false;
          }
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(ptrLocal)});

          const int32_t indexLocal = allocTempLocal();
          if (!emitExpr(expr.args[1], localsIn)) {
            return false;
          }
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(indexLocal)});

          if (accessName == "at") {
            const int32_t countLocal = allocTempLocal();
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
            function.instructions.push_back({IrOpcode::LoadIndirect, 0});
            function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(countLocal)});

            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
            function.instructions.push_back({IrOpcode::PushI32, 0});
            function.instructions.push_back({IrOpcode::CmpLtI32, 0});
            size_t jumpNonNegative = function.instructions.size();
            function.instructions.push_back({IrOpcode::JumpIfZero, 0});
            emitArrayIndexOutOfBounds();
            size_t nonNegativeIndex = function.instructions.size();
            function.instructions[jumpNonNegative].imm = static_cast<int32_t>(nonNegativeIndex);

            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal)});
            function.instructions.push_back({IrOpcode::CmpGeI32, 0});
            size_t jumpInRange = function.instructions.size();
            function.instructions.push_back({IrOpcode::JumpIfZero, 0});
            emitArrayIndexOutOfBounds();
            size_t inRangeIndex = function.instructions.size();
            function.instructions[jumpInRange].imm = static_cast<int32_t>(inRangeIndex);
          }

          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
          function.instructions.push_back({IrOpcode::PushI32, 1});
          function.instructions.push_back({IrOpcode::AddI32, 0});
          function.instructions.push_back({IrOpcode::PushI32, 16});
          function.instructions.push_back({IrOpcode::MulI32, 0});
          function.instructions.push_back({IrOpcode::AddI64, 0});
          function.instructions.push_back({IrOpcode::LoadIndirect, 0});
          return true;
        }
        std::string builtin;
        if (getBuiltinOperatorName(expr, builtin)) {
          if (builtin == "negate") {
            if (expr.args.size() != 1) {
              error = "negate requires exactly one argument";
              return false;
            }
            if (!emitExpr(expr.args.front(), localsIn)) {
              return false;
            }
            LocalInfo::ValueKind kind = inferExprKind(expr.args.front(), localsIn);
            if (kind == LocalInfo::ValueKind::Bool || kind == LocalInfo::ValueKind::Unknown) {
              error = "negate requires numeric operand";
              return false;
            }
            if (kind == LocalInfo::ValueKind::UInt64) {
              error = "negate does not support unsigned operands";
              return false;
            }
            function.instructions.push_back({kind == LocalInfo::ValueKind::Int64 ? IrOpcode::NegI64 : IrOpcode::NegI32,
                                             0});
            return true;
          }
          if (expr.args.size() != 2) {
            error = builtin + " requires exactly two arguments";
            return false;
          }
          std::function<bool(const Expr &, const LocalMap &)> isPointerOperand;
          isPointerOperand = [&](const Expr &candidate, const LocalMap &localsRef) -> bool {
            if (candidate.kind == Expr::Kind::Name) {
              auto it = localsRef.find(candidate.name);
              return it != localsRef.end() && it->second.kind == LocalInfo::Kind::Pointer;
            }
            if (candidate.kind == Expr::Kind::Call && isSimpleCallName(candidate, "location")) {
              return true;
            }
            if (candidate.kind == Expr::Kind::Call) {
              std::string opName;
              if (getBuiltinOperatorName(candidate, opName) && (opName == "plus" || opName == "minus") &&
                  candidate.args.size() == 2) {
                return isPointerOperand(candidate.args[0], localsRef) && !isPointerOperand(candidate.args[1], localsRef);
              }
            }
            return false;
          };
          bool leftPointer = false;
          bool rightPointer = false;
          if (builtin == "plus" || builtin == "minus") {
            leftPointer = isPointerOperand(expr.args[0], localsIn);
            rightPointer = isPointerOperand(expr.args[1], localsIn);
            if (leftPointer && rightPointer) {
              error = "pointer arithmetic does not support pointer + pointer";
              return false;
            }
            if (rightPointer) {
              error = "pointer arithmetic requires pointer on the left";
              return false;
            }
            if (leftPointer || rightPointer) {
              const Expr &offsetExpr = leftPointer ? expr.args[1] : expr.args[0];
              LocalInfo::ValueKind offsetKind = inferExprKind(offsetExpr, localsIn);
              if (offsetKind != LocalInfo::ValueKind::Int32 && offsetKind != LocalInfo::ValueKind::Int64 &&
                  offsetKind != LocalInfo::ValueKind::UInt64) {
                error = "pointer arithmetic requires an integer offset";
                return false;
              }
            }
          }
          if (!emitExpr(expr.args[0], localsIn)) {
            return false;
          }
          if (!emitExpr(expr.args[1], localsIn)) {
            return false;
          }
          LocalInfo::ValueKind numericKind =
              combineNumericKinds(inferExprKind(expr.args[0], localsIn), inferExprKind(expr.args[1], localsIn));
          if (numericKind == LocalInfo::ValueKind::Unknown && !(leftPointer || rightPointer)) {
            error = "unsupported operand types for " + builtin;
            return false;
          }
          IrOpcode op = IrOpcode::AddI32;
          if (builtin == "plus") {
            if (leftPointer || rightPointer) {
              op = IrOpcode::AddI64;
            } else if (numericKind == LocalInfo::ValueKind::Int64 ||
                       numericKind == LocalInfo::ValueKind::UInt64) {
              op = IrOpcode::AddI64;
            } else {
              op = IrOpcode::AddI32;
            }
          } else if (builtin == "minus") {
            if (leftPointer || rightPointer) {
              op = IrOpcode::SubI64;
            } else if (numericKind == LocalInfo::ValueKind::Int64 ||
                       numericKind == LocalInfo::ValueKind::UInt64) {
              op = IrOpcode::SubI64;
            } else {
              op = IrOpcode::SubI32;
            }
          } else if (builtin == "multiply") {
            op = (numericKind == LocalInfo::ValueKind::Int64 || numericKind == LocalInfo::ValueKind::UInt64)
                     ? IrOpcode::MulI64
                     : IrOpcode::MulI32;
          } else if (builtin == "divide") {
            if (numericKind == LocalInfo::ValueKind::UInt64) {
              op = IrOpcode::DivU64;
            } else if (numericKind == LocalInfo::ValueKind::Int64) {
              op = IrOpcode::DivI64;
            } else {
              op = IrOpcode::DivI32;
            }
          }
          function.instructions.push_back({op, 0});
          return true;
        }
        if (getBuiltinComparisonName(expr, builtin)) {
          if (builtin == "not") {
            if (expr.args.size() != 1) {
              error = "not requires exactly one argument";
              return false;
            }
            if (!emitExpr(expr.args.front(), localsIn)) {
              return false;
            }
            LocalInfo::ValueKind kind = inferExprKind(expr.args.front(), localsIn);
            return emitCompareToZero(kind, true);
          }
          if (builtin == "and") {
            if (expr.args.size() != 2) {
              error = "and requires exactly two arguments";
              return false;
            }
            if (!emitExpr(expr.args[0], localsIn)) {
              return false;
            }
            LocalInfo::ValueKind leftKind = inferExprKind(expr.args[0], localsIn);
            if (!emitCompareToZero(leftKind, false)) {
              return false;
            }
            size_t jumpFalse = function.instructions.size();
            function.instructions.push_back({IrOpcode::JumpIfZero, 0});
            if (!emitExpr(expr.args[1], localsIn)) {
              return false;
            }
            LocalInfo::ValueKind rightKind = inferExprKind(expr.args[1], localsIn);
            if (!emitCompareToZero(rightKind, false)) {
              return false;
            }
            size_t jumpEnd = function.instructions.size();
            function.instructions.push_back({IrOpcode::Jump, 0});
            size_t falseIndex = function.instructions.size();
            function.instructions[jumpFalse].imm = static_cast<int32_t>(falseIndex);
            function.instructions.push_back({IrOpcode::PushI32, 0});
            size_t endIndex = function.instructions.size();
            function.instructions[jumpEnd].imm = static_cast<int32_t>(endIndex);
            return true;
          }
          if (builtin == "or") {
            if (expr.args.size() != 2) {
              error = "or requires exactly two arguments";
              return false;
            }
            if (!emitExpr(expr.args[0], localsIn)) {
              return false;
            }
            LocalInfo::ValueKind leftKind = inferExprKind(expr.args[0], localsIn);
            if (!emitCompareToZero(leftKind, false)) {
              return false;
            }
            size_t jumpEval = function.instructions.size();
            function.instructions.push_back({IrOpcode::JumpIfZero, 0});
            function.instructions.push_back({IrOpcode::PushI32, 1});
            size_t jumpEnd = function.instructions.size();
            function.instructions.push_back({IrOpcode::Jump, 0});
            size_t evalIndex = function.instructions.size();
            function.instructions[jumpEval].imm = static_cast<int32_t>(evalIndex);
            if (!emitExpr(expr.args[1], localsIn)) {
              return false;
            }
            LocalInfo::ValueKind rightKind = inferExprKind(expr.args[1], localsIn);
            if (!emitCompareToZero(rightKind, false)) {
              return false;
            }
            size_t endIndex = function.instructions.size();
            function.instructions[jumpEnd].imm = static_cast<int32_t>(endIndex);
            return true;
          }
          if (expr.args.size() != 2) {
            error = builtin + " requires exactly two arguments";
            return false;
          }
          if (!emitExpr(expr.args[0], localsIn)) {
            return false;
          }
          if (!emitExpr(expr.args[1], localsIn)) {
            return false;
          }
          LocalInfo::ValueKind numericKind =
              comparisonKind(inferExprKind(expr.args[0], localsIn), inferExprKind(expr.args[1], localsIn));
          if (numericKind == LocalInfo::ValueKind::Unknown) {
            error = "unsupported operand types for " + builtin;
            return false;
          }
          IrOpcode op = IrOpcode::CmpEqI32;
          if (builtin == "equal") {
            op = (numericKind == LocalInfo::ValueKind::UInt64 || numericKind == LocalInfo::ValueKind::Int64)
                     ? IrOpcode::CmpEqI64
                     : IrOpcode::CmpEqI32;
          } else if (builtin == "not_equal") {
            op = (numericKind == LocalInfo::ValueKind::UInt64 || numericKind == LocalInfo::ValueKind::Int64)
                     ? IrOpcode::CmpNeI64
                     : IrOpcode::CmpNeI32;
          } else if (builtin == "less_than") {
            if (numericKind == LocalInfo::ValueKind::UInt64) {
              op = IrOpcode::CmpLtU64;
            } else if (numericKind == LocalInfo::ValueKind::Int64) {
              op = IrOpcode::CmpLtI64;
            } else {
              op = IrOpcode::CmpLtI32;
            }
          } else if (builtin == "less_equal") {
            if (numericKind == LocalInfo::ValueKind::UInt64) {
              op = IrOpcode::CmpLeU64;
            } else if (numericKind == LocalInfo::ValueKind::Int64) {
              op = IrOpcode::CmpLeI64;
            } else {
              op = IrOpcode::CmpLeI32;
            }
          } else if (builtin == "greater_than") {
            if (numericKind == LocalInfo::ValueKind::UInt64) {
              op = IrOpcode::CmpGtU64;
            } else if (numericKind == LocalInfo::ValueKind::Int64) {
              op = IrOpcode::CmpGtI64;
            } else {
              op = IrOpcode::CmpGtI32;
            }
          } else if (builtin == "greater_equal") {
            if (numericKind == LocalInfo::ValueKind::UInt64) {
              op = IrOpcode::CmpGeU64;
            } else if (numericKind == LocalInfo::ValueKind::Int64) {
              op = IrOpcode::CmpGeI64;
            } else {
              op = IrOpcode::CmpGeI32;
            }
          }
          function.instructions.push_back({op, 0});
          return true;
        }
        if (getBuiltinClampName(expr)) {
          if (expr.args.size() != 3) {
            error = "clamp requires exactly three arguments";
            return false;
          }
          bool sawUnsigned = false;
          bool sawSigned = false;
          for (const auto &arg : expr.args) {
            LocalInfo::ValueKind kind = inferExprKind(arg, localsIn);
            if (arg.kind == Expr::Kind::Literal && arg.isUnsigned) {
              sawUnsigned = true;
            }
            if (kind == LocalInfo::ValueKind::UInt64) {
              sawUnsigned = true;
            } else if (kind == LocalInfo::ValueKind::Int32 || kind == LocalInfo::ValueKind::Int64) {
              sawSigned = true;
            }
          }
          if (sawUnsigned && sawSigned) {
            error = "clamp requires numeric arguments of the same type";
            return false;
          }
          LocalInfo::ValueKind clampKind =
              combineNumericKinds(combineNumericKinds(inferExprKind(expr.args[0], localsIn),
                                                      inferExprKind(expr.args[1], localsIn)),
                                  inferExprKind(expr.args[2], localsIn));
          if (clampKind == LocalInfo::ValueKind::Unknown) {
            error = "clamp requires numeric arguments of the same type";
            return false;
          }
          IrOpcode cmpLt = IrOpcode::CmpLtI32;
          IrOpcode cmpGt = IrOpcode::CmpGtI32;
          if (clampKind == LocalInfo::ValueKind::UInt64) {
            cmpLt = IrOpcode::CmpLtU64;
            cmpGt = IrOpcode::CmpGtU64;
          } else if (clampKind == LocalInfo::ValueKind::Int64) {
            cmpLt = IrOpcode::CmpLtI64;
            cmpGt = IrOpcode::CmpGtI64;
          }
          int32_t tempValue = allocTempLocal();
          int32_t tempMin = allocTempLocal();
          int32_t tempMax = allocTempLocal();
          int32_t tempOut = allocTempLocal();
          if (!emitExpr(expr.args[0], localsIn)) {
            return false;
          }
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempValue)});
          if (!emitExpr(expr.args[1], localsIn)) {
            return false;
          }
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempMin)});
          if (!emitExpr(expr.args[2], localsIn)) {
            return false;
          }
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempMax)});

          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValue)});
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempMin)});
          function.instructions.push_back({cmpLt, 0});
          size_t skipMin = function.instructions.size();
          function.instructions.push_back({IrOpcode::JumpIfZero, 0});
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempMin)});
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOut)});
          size_t jumpToEnd = function.instructions.size();
          function.instructions.push_back({IrOpcode::Jump, 0});
          size_t checkMax = function.instructions.size();
          function.instructions[skipMin].imm = static_cast<int32_t>(checkMax);

          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValue)});
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempMax)});
          function.instructions.push_back({cmpGt, 0});
          size_t skipMax = function.instructions.size();
          function.instructions.push_back({IrOpcode::JumpIfZero, 0});
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempMax)});
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOut)});
          size_t jumpToEnd2 = function.instructions.size();
          function.instructions.push_back({IrOpcode::Jump, 0});
          size_t useValue = function.instructions.size();
          function.instructions[skipMax].imm = static_cast<int32_t>(useValue);
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValue)});
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOut)});
          size_t endIndex = function.instructions.size();
          function.instructions[jumpToEnd].imm = static_cast<int32_t>(endIndex);
          function.instructions[jumpToEnd2].imm = static_cast<int32_t>(endIndex);
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempOut)});
          return true;
        }
        if (getBuiltinConvertName(expr)) {
          if (expr.templateArgs.size() != 1) {
            error = "convert requires exactly one template argument";
            return false;
          }
          if (expr.args.size() != 1) {
            error = "convert requires exactly one argument";
            return false;
          }
          const std::string &typeName = expr.templateArgs.front();
          if (typeName != "int" && typeName != "i32" && typeName != "i64" && typeName != "u64" && typeName != "bool") {
            error = "native backend only supports convert<int>, convert<i32>, convert<i64>, convert<u64>, or "
                    "convert<bool>";
            return false;
          }
          if (!emitExpr(expr.args.front(), localsIn)) {
            return false;
          }
          if (typeName == "bool") {
            LocalInfo::ValueKind kind = inferExprKind(expr.args.front(), localsIn);
            if (!emitCompareToZero(kind, false)) {
              return false;
            }
          }
          return true;
        }
        if (getBuiltinPointerName(expr, builtin)) {
          if (expr.args.size() != 1) {
            error = builtin + " requires exactly one argument";
            return false;
          }
          if (builtin == "location") {
            const Expr &target = expr.args.front();
            if (target.kind != Expr::Kind::Name) {
              error = "location requires a local binding";
              return false;
            }
            auto it = localsIn.find(target.name);
            if (it == localsIn.end()) {
              error = "location requires a local binding";
              return false;
            }
            if (it->second.kind == LocalInfo::Kind::Reference) {
              function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index)});
            } else {
              function.instructions.push_back({IrOpcode::AddressOfLocal, static_cast<uint64_t>(it->second.index)});
            }
            return true;
          }
          const Expr &pointerExpr = expr.args.front();
          if (pointerExpr.kind == Expr::Kind::Name) {
            auto it = localsIn.find(pointerExpr.name);
            if (it == localsIn.end()) {
              error = "native backend does not know identifier: " + pointerExpr.name;
              return false;
            }
            if (it->second.kind != LocalInfo::Kind::Pointer && it->second.kind != LocalInfo::Kind::Reference) {
              error = "dereference requires a pointer or reference";
              return false;
            }
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index)});
          } else {
            if (!emitExpr(pointerExpr, localsIn)) {
              return false;
            }
          }
          function.instructions.push_back({IrOpcode::LoadIndirect, 0});
          return true;
        }
        if (getBuiltinCollectionName(expr, builtin)) {
          if (builtin == "array") {
            if (expr.templateArgs.size() != 1) {
              error = "array literal requires exactly one template argument";
              return false;
            }
            LocalInfo::ValueKind elemKind = valueKindFromTypeName(expr.templateArgs.front());
            if (elemKind == LocalInfo::ValueKind::Unknown || elemKind == LocalInfo::ValueKind::String) {
              error = "native backend only supports numeric/bool array literals";
              return false;
            }
            if (expr.args.size() > static_cast<size_t>(std::numeric_limits<int32_t>::max())) {
              error = "array literal too large for native backend";
              return false;
            }

            const int32_t baseLocal = nextLocal;
            nextLocal += static_cast<int32_t>(1 + expr.args.size());

            function.instructions.push_back({IrOpcode::PushI32, static_cast<uint64_t>(static_cast<int32_t>(expr.args.size()))});
            function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(baseLocal)});

            for (size_t i = 0; i < expr.args.size(); ++i) {
              const Expr &arg = expr.args[i];
              LocalInfo::ValueKind argKind = inferExprKind(arg, localsIn);
              if (argKind == LocalInfo::ValueKind::Unknown || argKind == LocalInfo::ValueKind::String) {
                error = "native backend requires array literal elements to be numeric/bool values";
                return false;
              }
              if (argKind != elemKind) {
                error = "array literal element type mismatch";
                return false;
              }
              if (!emitExpr(arg, localsIn)) {
                return false;
              }
              function.instructions.push_back(
                  {IrOpcode::StoreLocal, static_cast<uint64_t>(baseLocal + 1 + static_cast<int32_t>(i))});
            }

            function.instructions.push_back({IrOpcode::AddressOfLocal, static_cast<uint64_t>(baseLocal)});
            return true;
          }
          error = "native backend does not support " + builtin + " literals";
          return false;
        }
        if (isSimpleCallName(expr, "assign")) {
          if (expr.args.size() != 2) {
            error = "assign requires exactly two arguments";
            return false;
          }
          const Expr &target = expr.args.front();
          if (target.kind == Expr::Kind::Name) {
            auto it = localsIn.find(target.name);
            if (it == localsIn.end()) {
              error = "assign target must be a known binding: " + target.name;
              return false;
            }
            if (!it->second.isMutable) {
              error = "assign target must be mutable: " + target.name;
              return false;
            }
            if (it->second.kind == LocalInfo::Kind::Reference) {
              function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index)});
              if (!emitExpr(expr.args[1], localsIn)) {
                return false;
              }
              function.instructions.push_back({IrOpcode::StoreIndirect, 0});
              return true;
            }
            if (!emitExpr(expr.args[1], localsIn)) {
              return false;
            }
            function.instructions.push_back({IrOpcode::Dup, 0});
            function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(it->second.index)});
            return true;
          }
          if (target.kind == Expr::Kind::Call && isSimpleCallName(target, "dereference")) {
            if (target.args.size() != 1) {
              error = "dereference requires exactly one argument";
              return false;
            }
            const Expr &pointerExpr = target.args.front();
            if (pointerExpr.kind == Expr::Kind::Name) {
              auto it = localsIn.find(pointerExpr.name);
              if (it == localsIn.end() || !it->second.isMutable) {
                error = "assign target must be a mutable binding";
                return false;
              }
              if (it->second.kind != LocalInfo::Kind::Pointer && it->second.kind != LocalInfo::Kind::Reference) {
                error = "assign target must be a mutable pointer binding";
                return false;
              }
              function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index)});
            } else {
              if (!emitExpr(pointerExpr, localsIn)) {
                return false;
              }
            }
            if (!emitExpr(expr.args[1], localsIn)) {
              return false;
            }
            function.instructions.push_back({IrOpcode::StoreIndirect, 0});
            return true;
          }
          error = "native backend only supports assign to local names or dereference";
          return false;
        }
        if (isIfCall(expr) || isThenCall(expr) || isElseCall(expr)) {
          error = "native backend does not support if/then/else in expression context";
          return false;
        }
        error = "native backend only supports arithmetic/comparison/clamp/convert/pointer/assign calls in expressions";
        return false;
      }
      default:
        error = "native backend only supports literals, names, and calls";
        return false;
    }
  };

  auto emitPrintArg = [&](const Expr &arg, const LocalMap &localsIn, const PrintBuiltin &builtin) -> bool {
    uint64_t flags = encodePrintFlags(builtin.newline, builtin.target == PrintTarget::Err);
    if (arg.kind == Expr::Kind::Call) {
      std::string accessName;
      if (getBuiltinArrayAccessName(arg, accessName)) {
        if (arg.args.size() != 2) {
          error = accessName + " requires exactly two arguments";
          return false;
        }
        if (isEntryArgsName(arg.args.front(), localsIn)) {
          LocalInfo::ValueKind indexKind = inferExprKind(arg.args[1], localsIn);
          if (indexKind == LocalInfo::ValueKind::Bool) {
            indexKind = LocalInfo::ValueKind::Int32;
          }
          if (indexKind != LocalInfo::ValueKind::Int32) {
            error = "native backend only supports i32 indices for " + accessName;
            return false;
          }

          const int32_t indexLocal = allocTempLocal();
          if (!emitExpr(arg.args[1], localsIn)) {
            return false;
          }
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(indexLocal)});

          if (accessName == "at") {
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
            function.instructions.push_back({IrOpcode::PushI32, 0});
            function.instructions.push_back({IrOpcode::CmpLtI32, 0});
            size_t jumpNonNegative = function.instructions.size();
            function.instructions.push_back({IrOpcode::JumpIfZero, 0});
            emitArrayIndexOutOfBounds();
            size_t nonNegativeIndex = function.instructions.size();
            function.instructions[jumpNonNegative].imm = static_cast<int32_t>(nonNegativeIndex);

            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
            function.instructions.push_back({IrOpcode::PushArgc, 0});
            function.instructions.push_back({IrOpcode::CmpGeI32, 0});
            size_t jumpInRange = function.instructions.size();
            function.instructions.push_back({IrOpcode::JumpIfZero, 0});
            emitArrayIndexOutOfBounds();
            size_t inRangeIndex = function.instructions.size();
            function.instructions[jumpInRange].imm = static_cast<int32_t>(inRangeIndex);
          }

          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
          function.instructions.push_back({IrOpcode::PrintArgv, flags});
          return true;
        }
      }
    }
    if (arg.kind == Expr::Kind::StringLiteral) {
      std::string decoded;
      if (!parseStringLiteral(arg.stringValue, decoded)) {
        return false;
      }
      int32_t index = internString(decoded);
      function.instructions.push_back(
          {IrOpcode::PrintString, encodePrintStringImm(static_cast<uint64_t>(index), flags)});
      return true;
    }
    if (arg.kind == Expr::Kind::Name) {
      auto it = localsIn.find(arg.name);
      if (it == localsIn.end()) {
        error = "native backend does not know identifier: " + arg.name;
        return false;
      }
      if (it->second.valueKind == LocalInfo::ValueKind::String) {
        if (it->second.stringSource == LocalInfo::StringSource::TableIndex) {
          if (it->second.stringIndex < 0) {
            error = "native backend missing string data for: " + arg.name;
            return false;
          }
          function.instructions.push_back(
              {IrOpcode::PrintString, encodePrintStringImm(static_cast<uint64_t>(it->second.stringIndex), flags)});
          return true;
        }
        if (it->second.stringSource == LocalInfo::StringSource::ArgvIndex) {
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index)});
          function.instructions.push_back({IrOpcode::PrintArgv, flags});
          return true;
        }
      }
    }
    if (!emitExpr(arg, localsIn)) {
      return false;
    }
    LocalInfo::ValueKind kind = inferExprKind(arg, localsIn);
    if (kind == LocalInfo::ValueKind::Int64) {
      function.instructions.push_back({IrOpcode::PrintI64, flags});
      return true;
    }
    if (kind == LocalInfo::ValueKind::UInt64) {
      function.instructions.push_back({IrOpcode::PrintU64, flags});
      return true;
    }
    if (kind == LocalInfo::ValueKind::Int32 || kind == LocalInfo::ValueKind::Bool) {
      function.instructions.push_back({IrOpcode::PrintI32, flags});
      return true;
    }
    error = builtin.name + " requires a numeric/bool or string literal/binding argument";
    return false;
  };

  emitStatement = [&](const Expr &stmt, LocalMap &localsIn) -> bool {
    if (stmt.isBinding) {
      if (stmt.args.size() != 1) {
        error = "binding requires exactly one argument";
        return false;
      }
      if (localsIn.count(stmt.name) > 0) {
        error = "binding redefines existing name: " + stmt.name;
        return false;
      }
      if (isFloatBinding(stmt)) {
        error = "native backend does not support float types";
        return false;
      }
      if (isStringBinding(stmt)) {
        if (bindingKind(stmt) != LocalInfo::Kind::Value) {
          error = "native backend does not support string pointers or references";
          return false;
        }
        const Expr &init = stmt.args.front();
        int32_t index = -1;
        LocalInfo::StringSource source = LocalInfo::StringSource::None;
        if (init.kind == Expr::Kind::StringLiteral) {
          std::string decoded;
          if (!parseStringLiteral(init.stringValue, decoded)) {
            return false;
          }
          index = internString(decoded);
          source = LocalInfo::StringSource::TableIndex;
        } else if (init.kind == Expr::Kind::Name) {
          auto it = localsIn.find(init.name);
          if (it == localsIn.end()) {
            error = "native backend does not know identifier: " + init.name;
            return false;
          }
          if (it->second.valueKind != LocalInfo::ValueKind::String ||
              it->second.stringSource == LocalInfo::StringSource::None) {
            error = "native backend requires string bindings to use string literals, bindings, or entry args";
            return false;
          }
          source = it->second.stringSource;
          index = it->second.stringIndex;
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index)});
        } else if (init.kind == Expr::Kind::Call) {
          std::string accessName;
          if (!getBuiltinArrayAccessName(init, accessName)) {
            error = "native backend requires string bindings to use string literals, bindings, or entry args";
            return false;
          }
          if (init.args.size() != 2) {
            error = accessName + " requires exactly two arguments";
            return false;
          }
          if (!isEntryArgsName(init.args.front(), localsIn)) {
            error = "native backend only supports entry argument indexing";
            return false;
          }
          LocalInfo::ValueKind indexKind = inferExprKind(init.args[1], localsIn);
          if (indexKind == LocalInfo::ValueKind::Bool) {
            indexKind = LocalInfo::ValueKind::Int32;
          }
          if (indexKind != LocalInfo::ValueKind::Int32) {
            error = "native backend only supports i32 indices for " + accessName;
            return false;
          }

          const int32_t argvIndexLocal = allocTempLocal();
          if (!emitExpr(init.args[1], localsIn)) {
            return false;
          }
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(argvIndexLocal)});

          if (accessName == "at") {
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(argvIndexLocal)});
            function.instructions.push_back({IrOpcode::PushI32, 0});
            function.instructions.push_back({IrOpcode::CmpLtI32, 0});
            size_t jumpNonNegative = function.instructions.size();
            function.instructions.push_back({IrOpcode::JumpIfZero, 0});
            emitArrayIndexOutOfBounds();
            size_t nonNegativeIndex = function.instructions.size();
            function.instructions[jumpNonNegative].imm = static_cast<int32_t>(nonNegativeIndex);

            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(argvIndexLocal)});
            function.instructions.push_back({IrOpcode::PushArgc, 0});
            function.instructions.push_back({IrOpcode::CmpGeI32, 0});
            size_t jumpInRange = function.instructions.size();
            function.instructions.push_back({IrOpcode::JumpIfZero, 0});
            emitArrayIndexOutOfBounds();
            size_t inRangeIndex = function.instructions.size();
            function.instructions[jumpInRange].imm = static_cast<int32_t>(inRangeIndex);
          }

          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(argvIndexLocal)});
          source = LocalInfo::StringSource::ArgvIndex;
        } else {
          error = "native backend requires string bindings to use string literals, bindings, or entry args";
          return false;
        }
        LocalInfo info;
        info.index = nextLocal++;
        info.isMutable = isBindingMutable(stmt);
        info.kind = LocalInfo::Kind::Value;
        info.valueKind = LocalInfo::ValueKind::String;
        info.stringSource = source;
        info.stringIndex = index;
        if (init.kind == Expr::Kind::StringLiteral) {
          function.instructions.push_back({IrOpcode::PushI64, static_cast<uint64_t>(index)});
        }
        localsIn.emplace(stmt.name, info);
        function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(info.index)});
        return true;
      }
      if (!emitExpr(stmt.args.front(), localsIn)) {
        return false;
      }
      LocalInfo info;
      info.index = nextLocal++;
      info.isMutable = isBindingMutable(stmt);
      info.kind = bindingKind(stmt);
      info.valueKind = bindingValueKind(stmt, info.kind);
      if (info.kind == LocalInfo::Kind::Reference) {
        const Expr &init = stmt.args.front();
        if (init.kind != Expr::Kind::Call || !isSimpleCallName(init, "location") || init.args.size() != 1) {
          error = "reference binding requires location(...) initializer";
          return false;
        }
      }
      localsIn.emplace(stmt.name, info);
      function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(info.index)});
      return true;
    }
    PrintBuiltin printBuiltin;
    if (stmt.kind == Expr::Kind::Call && getPrintBuiltin(stmt, printBuiltin)) {
      if (!stmt.bodyArguments.empty()) {
        error = printBuiltin.name + " does not support body arguments";
        return false;
      }
      if (stmt.args.size() != 1) {
        error = printBuiltin.name + " requires exactly one argument";
        return false;
      }
      return emitPrintArg(stmt.args.front(), localsIn, printBuiltin);
    }
    if (isReturnCall(stmt)) {
      if (activeInlineContext) {
        InlineContext &context = *activeInlineContext;
        if (stmt.args.empty()) {
          if (!context.returnsVoid) {
            error = "return requires exactly one argument";
            return false;
          }
          size_t jumpIndex = function.instructions.size();
          function.instructions.push_back({IrOpcode::Jump, 0});
          context.returnJumps.push_back(jumpIndex);
          return true;
        }
        if (stmt.args.size() != 1) {
          error = "return requires exactly one argument";
          return false;
        }
        if (context.returnsVoid) {
          error = "return value not allowed for void definition";
          return false;
        }
        if (context.returnLocal < 0) {
          error = "native backend missing inline return local";
          return false;
        }
        if (!emitExpr(stmt.args.front(), localsIn)) {
          return false;
        }
        LocalInfo::ValueKind returnKind = inferExprKind(stmt.args.front(), localsIn);
        if (returnKind == LocalInfo::ValueKind::Int64 || returnKind == LocalInfo::ValueKind::UInt64 ||
            returnKind == LocalInfo::ValueKind::Int32 || returnKind == LocalInfo::ValueKind::Bool) {
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(context.returnLocal)});
          size_t jumpIndex = function.instructions.size();
          function.instructions.push_back({IrOpcode::Jump, 0});
          context.returnJumps.push_back(jumpIndex);
          return true;
        }
        error = "native backend only supports returning numeric or bool values";
        return false;
      }

      if (stmt.args.empty()) {
        if (!returnsVoid) {
          error = "return requires exactly one argument";
          return false;
        }
        function.instructions.push_back({IrOpcode::ReturnVoid, 0});
        sawReturn = true;
        return true;
      }
      if (stmt.args.size() != 1) {
        error = "return requires exactly one argument";
        return false;
      }
      if (returnsVoid) {
        error = "return value not allowed for void definition";
        return false;
      }
      if (!emitExpr(stmt.args.front(), localsIn)) {
        return false;
      }
      LocalInfo::ValueKind returnKind = inferExprKind(stmt.args.front(), localsIn);
      if (returnKind == LocalInfo::ValueKind::Int64 || returnKind == LocalInfo::ValueKind::UInt64) {
        function.instructions.push_back({IrOpcode::ReturnI64, 0});
      } else if (returnKind == LocalInfo::ValueKind::Int32 || returnKind == LocalInfo::ValueKind::Bool) {
        function.instructions.push_back({IrOpcode::ReturnI32, 0});
      } else {
        error = "native backend only supports returning numeric or bool values";
        return false;
      }
      sawReturn = true;
      return true;
    }
    if (isIfCall(stmt)) {
      if (stmt.args.size() != 3) {
        error = "if requires condition, then, else";
        return false;
      }
      if (!stmt.bodyArguments.empty()) {
        error = "if does not accept trailing block arguments";
        return false;
      }
      if (!emitExpr(stmt.args[0], localsIn)) {
        return false;
      }
      const Expr &thenBlock = stmt.args[1];
      const Expr &elseBlock = stmt.args[2];
      if (!isThenCall(thenBlock) || !isElseCall(elseBlock)) {
        error = "if requires then/else blocks";
        return false;
      }
      size_t jumpIfZeroIndex = function.instructions.size();
      function.instructions.push_back({IrOpcode::JumpIfZero, 0});
      LocalMap thenLocals = localsIn;
      if (!emitBlock(thenBlock, thenLocals)) {
        return false;
      }
      size_t jumpIndex = function.instructions.size();
      function.instructions.push_back({IrOpcode::Jump, 0});
      size_t elseIndex = function.instructions.size();
      function.instructions[jumpIfZeroIndex].imm = static_cast<int32_t>(elseIndex);
      LocalMap elseLocals = localsIn;
      if (!emitBlock(elseBlock, elseLocals)) {
        return false;
      }
      size_t endIndex = function.instructions.size();
      function.instructions[jumpIndex].imm = static_cast<int32_t>(endIndex);
      return true;
    }
    if (isRepeatCall(stmt)) {
      if (stmt.args.size() != 1) {
        error = "repeat requires exactly one argument";
        return false;
      }
      if (!emitExpr(stmt.args.front(), localsIn)) {
        return false;
      }
      LocalInfo::ValueKind countKind = inferExprKind(stmt.args.front(), localsIn);
      if (countKind == LocalInfo::ValueKind::Bool) {
        countKind = LocalInfo::ValueKind::Int32;
      }
      if (countKind != LocalInfo::ValueKind::Int32 && countKind != LocalInfo::ValueKind::Int64 &&
          countKind != LocalInfo::ValueKind::UInt64) {
        error = "repeat count requires integer or bool";
        return false;
      }

      const int32_t counterLocal = allocTempLocal();
      function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(counterLocal)});

      const size_t checkIndex = function.instructions.size();
      function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(counterLocal)});
      if (countKind == LocalInfo::ValueKind::Int32) {
        function.instructions.push_back({IrOpcode::PushI32, 0});
        function.instructions.push_back({IrOpcode::CmpGtI32, 0});
      } else if (countKind == LocalInfo::ValueKind::Int64) {
        function.instructions.push_back({IrOpcode::PushI64, 0});
        function.instructions.push_back({IrOpcode::CmpGtI64, 0});
      } else {
        function.instructions.push_back({IrOpcode::PushI64, 0});
        function.instructions.push_back({IrOpcode::CmpNeI64, 0});
      }

      const size_t jumpEndIndex = function.instructions.size();
      function.instructions.push_back({IrOpcode::JumpIfZero, 0});

      LocalMap bodyLocals = localsIn;
      for (const auto &bodyStmt : stmt.bodyArguments) {
        if (!emitStatement(bodyStmt, bodyLocals)) {
          return false;
        }
      }

      function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(counterLocal)});
      if (countKind == LocalInfo::ValueKind::Int32) {
        function.instructions.push_back({IrOpcode::PushI32, 1});
        function.instructions.push_back({IrOpcode::SubI32, 0});
      } else {
        function.instructions.push_back({IrOpcode::PushI64, 1});
        function.instructions.push_back({IrOpcode::SubI64, 0});
      }
      function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(counterLocal)});
      function.instructions.push_back({IrOpcode::Jump, static_cast<uint64_t>(checkIndex)});

      const size_t endIndex = function.instructions.size();
      function.instructions[jumpEndIndex].imm = static_cast<int32_t>(endIndex);
      return true;
    }
    if (stmt.kind == Expr::Kind::Call) {
      if (const Definition *callee = resolveDefinitionCall(stmt)) {
        if (!stmt.bodyArguments.empty()) {
          error = "native backend does not support block arguments on calls";
          return false;
        }
        ReturnInfo info;
        if (!getReturnInfo(callee->fullPath, info)) {
          return false;
        }
        if (!emitInlineDefinitionCall(stmt, *callee, localsIn, false)) {
          return false;
        }
        if (!info.returnsVoid) {
          function.instructions.push_back({IrOpcode::Pop, 0});
        }
        return true;
      }
    }
    if (stmt.kind == Expr::Kind::Call && isSimpleCallName(stmt, "assign")) {
      if (!emitExpr(stmt, localsIn)) {
        return false;
      }
      function.instructions.push_back({IrOpcode::Pop, 0});
      return true;
    }
    if (!emitExpr(stmt, localsIn)) {
      return false;
    }
    function.instructions.push_back({IrOpcode::Pop, 0});
    return true;
  };

  for (const auto &stmt : entryDef->statements) {
    if (!emitStatement(stmt, locals)) {
      return false;
    }
  }

  if (!sawReturn) {
    if (returnsVoid) {
      function.instructions.push_back({IrOpcode::ReturnVoid, 0});
    } else {
      error = "native backend requires an explicit return statement";
      return false;
    }
  }

  out.functions.push_back(std::move(function));
  out.entryIndex = 0;
  out.stringTable = std::move(stringTable);
  return true;
}

} // namespace primec
