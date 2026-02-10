#include "EmitterHelpers.h"
#include "primec/StringLiteral.h"

#include <functional>

namespace primec::emitter {

using BindingInfo = Emitter::BindingInfo;
using ReturnKind = Emitter::ReturnKind;

bool getBuiltinOperator(const Expr &expr, char &out) {
  if (expr.name.empty()) {
    return false;
  }
  std::string name = expr.name;
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  if (name.find('/') != std::string::npos) {
    return false;
  }
  if (name == "plus") {
    out = '+';
    return true;
  }
  if (name == "minus") {
    out = '-';
    return true;
  }
  if (name == "multiply") {
    out = '*';
    return true;
  }
  if (name == "divide") {
    out = '/';
    return true;
  }
  return false;
}

bool getBuiltinComparison(const Expr &expr, const char *&out) {
  if (expr.name.empty()) {
    return false;
  }
  std::string name = expr.name;
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  if (name.find('/') != std::string::npos) {
    return false;
  }
  if (name == "greater_than") {
    out = ">";
    return true;
  }
  if (name == "less_than") {
    out = "<";
    return true;
  }
  if (name == "equal") {
    out = "==";
    return true;
  }
  if (name == "not_equal") {
    out = "!=";
    return true;
  }
  if (name == "greater_equal") {
    out = ">=";
    return true;
  }
  if (name == "less_equal") {
    out = "<=";
    return true;
  }
  if (name == "and") {
    out = "&&";
    return true;
  }
  if (name == "or") {
    out = "||";
    return true;
  }
  if (name == "not") {
    out = "!";
    return true;
  }
  return false;
}

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

bool isPathSpaceBuiltinName(const Expr &expr) {
  return isSimpleCallName(expr, "notify") || isSimpleCallName(expr, "insert") || isSimpleCallName(expr, "take");
}

std::string stripStringLiteralSuffix(const std::string &token) {
  std::string literalText;
  std::string suffix;
  if (!splitStringLiteralToken(token, literalText, suffix)) {
    return token;
  }
  bool raw = false;
  if (suffix == "raw_utf8" || suffix == "raw_ascii") {
    raw = true;
  } else if (suffix.empty() || suffix == "utf8" || suffix == "ascii") {
    raw = false;
  } else {
    return token;
  }

  // PrimeStruct string literals can use either 'single' or "double" quotes.
  // For C++ output we always emit a valid C++ string literal using double quotes,
  // escaping embedded quotes/backslashes and any decoded escape sequences.
  std::string decoded;
  std::string error;
  if (!decodeStringLiteralText(literalText, decoded, error, raw)) {
    return literalText;
  }

  std::string escaped;
  escaped.reserve(decoded.size() + 2);
  escaped.push_back('"');
  for (char c : decoded) {
    switch (c) {
      case '\\':
        escaped += "\\\\";
        break;
      case '"':
        escaped += "\\\"";
        break;
      case '\n':
        escaped += "\\n";
        break;
      case '\r':
        escaped += "\\r";
        break;
      case '\t':
        escaped += "\\t";
        break;
      case '\0':
        escaped += "\\0";
        break;
      default:
        escaped.push_back(c);
        break;
    }
  }
  escaped.push_back('"');
  return escaped;
}

bool isBuiltinNegate(const Expr &expr) {
  if (expr.name.empty()) {
    return false;
  }
  std::string name = expr.name;
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  if (name.find('/') != std::string::npos) {
    return false;
  }
  return name == "negate";
}

bool isBuiltinClamp(const Expr &expr) {
  if (expr.name.empty()) {
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

bool getBuiltinMinMaxName(const Expr &expr, std::string &out) {
  if (expr.name.empty()) {
    return false;
  }
  std::string name = expr.name;
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  if (name.find('/') != std::string::npos) {
    return false;
  }
  if (name == "min" || name == "max") {
    out = name;
    return true;
  }
  return false;
}

bool getBuiltinAbsSignName(const Expr &expr, std::string &out) {
  if (expr.name.empty()) {
    return false;
  }
  std::string name = expr.name;
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  if (name.find('/') != std::string::npos) {
    return false;
  }
  if (name == "abs" || name == "sign") {
    out = name;
    return true;
  }
  return false;
}

bool getBuiltinSaturateName(const Expr &expr, std::string &out) {
  if (expr.name.empty()) {
    return false;
  }
  std::string name = expr.name;
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  if (name.find('/') != std::string::npos) {
    return false;
  }
  if (name == "saturate") {
    out = name;
    return true;
  }
  return false;
}

bool getBuiltinMathName(const Expr &expr, std::string &out) {
  if (expr.name.empty()) {
    return false;
  }
  std::string name = expr.name;
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  if (name.find('/') != std::string::npos) {
    return false;
  }
  if (name == "lerp" || name == "floor" || name == "ceil" || name == "round" || name == "trunc" ||
      name == "fract" || name == "sqrt" || name == "cbrt" || name == "pow" || name == "exp" ||
      name == "exp2" || name == "log" || name == "log2" || name == "log10" || name == "sin" ||
      name == "cos" || name == "tan" || name == "asin" || name == "acos" || name == "atan" ||
      name == "atan2" || name == "radians" || name == "degrees" || name == "sinh" || name == "cosh" ||
      name == "tanh" || name == "asinh" || name == "acosh" || name == "atanh" || name == "fma" ||
      name == "hypot" || name == "copysign" || name == "is_nan" || name == "is_inf" ||
      name == "is_finite") {
    out = name;
    return true;
  }
  return false;
}

bool isBuiltinMathConstantName(const std::string &name) {
  return name == "pi" || name == "tau" || name == "e";
}

bool getBuiltinPointerOperator(const Expr &expr, char &out) {
  if (expr.name.empty()) {
    return false;
  }
  std::string name = expr.name;
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  if (name.find('/') != std::string::npos) {
    return false;
  }
  if (name == "dereference") {
    out = '*';
    return true;
  }
  if (name == "location") {
    out = '&';
    return true;
  }
  return false;
}

bool getBuiltinConvertName(const Expr &expr, std::string &out) {
  if (expr.name.empty()) {
    return false;
  }
  std::string name = expr.name;
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  if (name.find('/') != std::string::npos) {
    return false;
  }
  if (name == "convert") {
    out = name;
    return true;
  }
  return false;
}

bool getBuiltinCollectionName(const Expr &expr, std::string &out) {
  if (expr.name.empty()) {
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

std::string resolveExprPath(const Expr &expr) {
  if (!expr.name.empty() && expr.name[0] == '/') {
    return expr.name;
  }
  if (!expr.namespacePrefix.empty()) {
    return expr.namespacePrefix + "/" + expr.name;
  }
  return "/" + expr.name;
}

bool isArrayValue(const Expr &target, const std::unordered_map<std::string, BindingInfo> &localTypes) {
  if (target.kind == Expr::Kind::Name) {
    auto it = localTypes.find(target.name);
    return it != localTypes.end() && it->second.typeName == "array";
  }
  if (target.kind == Expr::Kind::Call) {
    std::string collection;
    return getBuiltinCollectionName(target, collection) && collection == "array" && target.templateArgs.size() == 1;
  }
  return false;
}

bool isMapValue(const Expr &target, const std::unordered_map<std::string, BindingInfo> &localTypes) {
  if (target.kind == Expr::Kind::Name) {
    auto it = localTypes.find(target.name);
    return it != localTypes.end() && it->second.typeName == "map";
  }
  if (target.kind == Expr::Kind::Call) {
    std::string collection;
    return getBuiltinCollectionName(target, collection) && collection == "map" && target.templateArgs.size() == 2;
  }
  return false;
}

bool isStringValue(const Expr &target, const std::unordered_map<std::string, BindingInfo> &localTypes) {
  if (target.kind == Expr::Kind::StringLiteral) {
    return true;
  }
  if (target.kind == Expr::Kind::Name) {
    auto it = localTypes.find(target.name);
    return it != localTypes.end() && it->second.typeName == "string";
  }
  if (target.kind == Expr::Kind::Call) {
    if ((target.name == "at" || target.name == "at_unsafe") && target.args.size() == 2) {
      const Expr &receiver = target.args.front();
      if (receiver.kind == Expr::Kind::Name) {
        auto it = localTypes.find(receiver.name);
        if (it != localTypes.end() && it->second.typeName == "array" && it->second.typeTemplateArg == "string") {
          return true;
        }
      }
      if (receiver.kind == Expr::Kind::Call) {
        std::string collection;
        if (getBuiltinCollectionName(receiver, collection) && collection == "array" && receiver.templateArgs.size() == 1 &&
            receiver.templateArgs.front() == "string") {
          return true;
        }
      }
    }
  }
  return false;
}

bool isArrayCountCall(const Expr &call, const std::unordered_map<std::string, BindingInfo> &localTypes) {
  if (!isSimpleCallName(call, "count") || call.args.size() != 1) {
    return false;
  }
  return isArrayValue(call.args.front(), localTypes);
}

bool isMapCountCall(const Expr &call, const std::unordered_map<std::string, BindingInfo> &localTypes) {
  if (!isSimpleCallName(call, "count") || call.args.size() != 1) {
    return false;
  }
  return isMapValue(call.args.front(), localTypes);
}

bool isStringCountCall(const Expr &call, const std::unordered_map<std::string, BindingInfo> &localTypes) {
  if (!isSimpleCallName(call, "count") || call.args.size() != 1) {
    return false;
  }
  return isStringValue(call.args.front(), localTypes);
}

bool resolveMethodCallPath(const Expr &call,
                           const std::unordered_map<std::string, BindingInfo> &localTypes,
                           const std::unordered_map<std::string, ReturnKind> &returnKinds,
                           std::string &resolvedOut) {
  if (!call.isMethodCall || call.args.empty()) {
    return false;
  }
  const Expr &receiver = call.args.front();
  std::function<std::string(const Expr &)> inferPrimitiveTypeName;
  inferPrimitiveTypeName = [&](const Expr &expr) -> std::string {
    switch (expr.kind) {
      case Expr::Kind::Literal:
        if (expr.isUnsigned) {
          return "u64";
        }
        return expr.intWidth == 64 ? "i64" : "i32";
      case Expr::Kind::BoolLiteral:
        return "bool";
      case Expr::Kind::FloatLiteral:
        return expr.floatWidth == 64 ? "f64" : "f32";
      case Expr::Kind::StringLiteral:
        return "";
      case Expr::Kind::Name: {
        auto it = localTypes.find(expr.name);
        if (it != localTypes.end() && isPrimitiveBindingTypeName(it->second.typeName)) {
          return normalizeBindingTypeName(it->second.typeName);
        }
        return "";
      }
      case Expr::Kind::Call: {
        if (isArrayCountCall(expr, localTypes)) {
          return "i32";
        }
        if (isMapCountCall(expr, localTypes)) {
          return "i32";
        }
        if (!expr.isMethodCall) {
          const std::string resolved = resolveExprPath(expr);
          auto it = returnKinds.find(resolved);
          if (it != returnKinds.end()) {
            return typeNameForReturnKind(it->second);
          }
          return "";
        }
        std::string methodPath;
        if (resolveMethodCallPath(expr, localTypes, returnKinds, methodPath)) {
          auto it = returnKinds.find(methodPath);
          if (it != returnKinds.end()) {
            return typeNameForReturnKind(it->second);
          }
        }
        return "";
      }
    }
    return "";
  };
  std::string typeName;
  if (receiver.kind == Expr::Kind::Name) {
    auto it = localTypes.find(receiver.name);
    if (it == localTypes.end()) {
      return false;
    }
    typeName = it->second.typeName;
  } else {
    typeName = inferPrimitiveTypeName(receiver);
  }
  if (typeName.empty()) {
    return false;
  }
  if (typeName == "Pointer" || typeName == "Reference") {
    return false;
  }
  if (isPrimitiveBindingTypeName(typeName)) {
    resolvedOut = "/" + normalizeBindingTypeName(typeName) + "/" + call.name;
    return true;
  }
  resolvedOut = resolveTypePath(typeName, call.namespacePrefix) + "/" + call.name;
  return true;
}

bool isBuiltinAssign(const Expr &expr, const std::unordered_map<std::string, std::string> &nameMap) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  std::string full = resolveExprPath(expr);
  if (nameMap.count(full) > 0) {
    return false;
  }
  std::string name = expr.name;
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  if (name.find('/') != std::string::npos) {
    return false;
  }
  return name == "assign";
}

std::vector<const Expr *> orderCallArguments(const Expr &expr, const std::vector<Expr> &params) {
  std::vector<const Expr *> ordered;
  ordered.reserve(expr.args.size());
  auto fallback = [&]() {
    ordered.clear();
    ordered.reserve(expr.args.size());
    for (const auto &arg : expr.args) {
      ordered.push_back(&arg);
    }
  };
  ordered.assign(params.size(), nullptr);
  size_t positionalIndex = 0;
  for (size_t i = 0; i < expr.args.size(); ++i) {
    if (i < expr.argNames.size() && expr.argNames[i].has_value()) {
      const std::string &name = *expr.argNames[i];
      size_t index = params.size();
      for (size_t p = 0; p < params.size(); ++p) {
        if (params[p].name == name) {
          index = p;
          break;
        }
      }
      if (index >= params.size()) {
        fallback();
        return ordered;
      }
      ordered[index] = &expr.args[i];
      continue;
    }
    while (positionalIndex < ordered.size() && ordered[positionalIndex] != nullptr) {
      ++positionalIndex;
    }
    if (positionalIndex >= ordered.size()) {
      fallback();
      return ordered;
    }
    ordered[positionalIndex] = &expr.args[i];
    ++positionalIndex;
  }
  for (size_t i = 0; i < ordered.size(); ++i) {
    if (ordered[i] != nullptr) {
      continue;
    }
    if (!params[i].args.empty()) {
      ordered[i] = &params[i].args.front();
    }
  }
  for (const auto *arg : ordered) {
    if (arg == nullptr) {
      fallback();
      return ordered;
    }
  }
  return ordered;
}

bool isBuiltinIf(const Expr &expr, const std::unordered_map<std::string, std::string> &nameMap) {
  std::string full = resolveExprPath(expr);
  if (nameMap.count(full) > 0) {
    return false;
  }
  return isSimpleCallName(expr, "if");
}

bool isBuiltinBlock(const Expr &expr, const std::unordered_map<std::string, std::string> &nameMap) {
  std::string full = resolveExprPath(expr);
  if (nameMap.count(full) > 0) {
    return false;
  }
  return isSimpleCallName(expr, "block");
}

bool isRepeatCall(const Expr &expr) {
  return isSimpleCallName(expr, "repeat");
}

bool hasNamedArguments(const std::vector<std::optional<std::string>> &argNames) {
  for (const auto &name : argNames) {
    if (name.has_value()) {
      return true;
    }
  }
  return false;
}

bool isReturnCall(const Expr &expr) {
  return isSimpleCallName(expr, "return");
}

} // namespace primec::emitter
