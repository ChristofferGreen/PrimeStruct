#include "EmitterHelpers.h"
#include "primec/StringLiteral.h"

#include <functional>

namespace primec::emitter {

using BindingInfo = Emitter::BindingInfo;
using ReturnKind = Emitter::ReturnKind;

namespace {

bool allowsArrayVectorCompatibilitySuffix(const std::string &suffix) {
  return suffix != "count" && suffix != "capacity" && suffix != "at" && suffix != "at_unsafe" &&
         suffix != "push" && suffix != "pop" && suffix != "reserve" && suffix != "clear" &&
         suffix != "remove_at" && suffix != "remove_swap";
}

bool allowsVectorStdlibCompatibilitySuffix(const std::string &suffix) {
  return suffix != "count" && suffix != "capacity" && suffix != "at" && suffix != "at_unsafe" &&
         suffix != "push" && suffix != "pop" && suffix != "reserve" && suffix != "clear" &&
         suffix != "remove_at" && suffix != "remove_swap";
}

bool getBuiltinArrayAccessNameLocal(const Expr &expr, std::string &out) {
  if (expr.name.empty()) {
    return false;
  }
  std::string name = expr.name;
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  if (name.rfind("std/collections/vector/", 0) == 0) {
    std::string alias = name.substr(std::string("std/collections/vector/").size());
    if (alias == "at" || alias == "at_unsafe") {
      out = alias;
      return true;
    }
    return false;
  }
  if (name.rfind("vector/", 0) == 0) {
    std::string alias = name.substr(std::string("vector/").size());
    if (alias == "at" || alias == "at_unsafe") {
      out = alias;
      return true;
    }
    return false;
  }
  if (name.rfind("array/", 0) == 0) {
    return false;
  }
  if (name.rfind("map/", 0) == 0) {
    std::string alias = name.substr(std::string("map/").size());
    if (alias == "at" || alias == "at_unsafe") {
      out = alias;
      return true;
    }
    return false;
  }
  if (name.rfind("std/collections/map/", 0) == 0) {
    std::string alias = name.substr(std::string("std/collections/map/").size());
    if (alias == "at" || alias == "at_unsafe") {
      out = alias;
      return true;
    }
    return false;
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

} // namespace

bool inferCollectionElementTypeNameFromBinding(const BindingInfo &binding, std::string &typeOut);
bool inferCollectionElementTypeNameFromExpr(const Expr &expr,
                                            const std::unordered_map<std::string, BindingInfo> &localTypes,
                                            const std::function<bool(const Expr &, std::string &)> &resolveCallElementTypeName,
                                            std::string &typeOut);

bool getBuiltinOperator(const Expr &expr, char &out) {
  if (expr.name.empty()) {
    return false;
  }
  std::string name = expr.name;
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  if (name.rfind("std/gpu/", 0) == 0) {
    name.erase(0, 8);
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

bool getBuiltinMutationName(const Expr &expr, std::string &out) {
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
  if (name == "increment" || name == "decrement") {
    out = name;
    return true;
  }
  return false;
}

bool isSimpleCallName(const Expr &expr, const char *nameToMatch) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  const std::string targetName = nameToMatch == nullptr ? std::string() : std::string(nameToMatch);
  std::string name = expr.name;
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  if (name.rfind("vector/", 0) == 0) {
    std::string alias = name.substr(std::string("vector/").size());
    if (alias.find('/') == std::string::npos && alias == "vector") {
      return alias == targetName;
    }
  }
  if (name.rfind("std/collections/vector/", 0) == 0) {
    std::string alias = name.substr(std::string("std/collections/vector/").size());
    if (alias.find('/') == std::string::npos &&
        (alias == "count" || alias == "capacity" || alias == "at" || alias == "at_unsafe" || alias == "push" ||
         alias == "pop" || alias == "reserve" || alias == "clear" || alias == "remove_at" ||
         alias == "remove_swap")) {
      return alias == targetName;
    }
  }
  if (name.rfind("map/", 0) == 0) {
    std::string alias = name.substr(std::string("map/").size());
    if (alias.find('/') == std::string::npos &&
        (alias == "map" || alias == "count" || alias == "at" || alias == "at_unsafe")) {
      return alias == targetName;
    }
  }
  if (name.rfind("std/collections/map/", 0) == 0) {
    std::string alias = name.substr(std::string("std/collections/map/").size());
    if (alias.find('/') == std::string::npos &&
        (alias == "map" || alias == "count" || alias == "at" || alias == "at_unsafe")) {
      return alias == targetName;
    }
  }
  if (name.find('/') != std::string::npos) {
    return false;
  }
  return name == targetName;
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

namespace {

bool parseMathName(const std::string &name, std::string &out, bool allowBare);

} // namespace

bool isBuiltinClamp(const Expr &expr, bool allowBare) {
  std::string name;
  if (!parseMathName(expr.name, name, allowBare)) {
    return false;
  }
  return name == "clamp";
}

bool getBuiltinMinMaxName(const Expr &expr, std::string &out, bool allowBare) {
  if (!parseMathName(expr.name, out, allowBare)) {
    return false;
  }
  return out == "min" || out == "max";
}

bool getBuiltinAbsSignName(const Expr &expr, std::string &out, bool allowBare) {
  if (!parseMathName(expr.name, out, allowBare)) {
    return false;
  }
  return out == "abs" || out == "sign";
}

bool getBuiltinSaturateName(const Expr &expr, std::string &out, bool allowBare) {
  if (!parseMathName(expr.name, out, allowBare)) {
    return false;
  }
  return out == "saturate";
}

namespace {

bool parseMathName(const std::string &name, std::string &out, bool allowBare) {
  if (name.empty()) {
    return false;
  }
  std::string normalized = name;
  if (!normalized.empty() && normalized[0] == '/') {
    normalized.erase(0, 1);
  }
  if (normalized.rfind("std/math/", 0) == 0) {
    out = normalized.substr(9);
    return true;
  }
  if (normalized.find('/') != std::string::npos) {
    return false;
  }
  if (!allowBare) {
    return false;
  }
  out = normalized;
  return true;
}

} // namespace

bool getBuiltinMathName(const Expr &expr, std::string &out, bool allowBare) {
  if (!parseMathName(expr.name, out, allowBare)) {
    return false;
  }
  if (out == "lerp" || out == "floor" || out == "ceil" || out == "round" || out == "trunc" || out == "fract" ||
      out == "sqrt" || out == "cbrt" || out == "pow" || out == "exp" || out == "exp2" || out == "log" ||
      out == "log2" || out == "log10" || out == "sin" || out == "cos" || out == "tan" || out == "asin" ||
      out == "acos" || out == "atan" || out == "atan2" || out == "radians" || out == "degrees" || out == "sinh" ||
      out == "cosh" || out == "tanh" || out == "asinh" || out == "acosh" || out == "atanh" || out == "fma" ||
      out == "hypot" || out == "copysign" || out == "is_nan" || out == "is_inf" || out == "is_finite") {
    return true;
  }
  return false;
}

bool isBuiltinMathConstantName(const std::string &name, bool allowBare) {
  std::string candidate;
  if (!parseMathName(name, candidate, allowBare)) {
    return false;
  }
  return candidate == "pi" || candidate == "tau" || candidate == "e";
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
  if (name.rfind("vector/", 0) == 0) {
    std::string alias = name.substr(std::string("vector/").size());
    if (alias == "vector") {
      out = "vector";
      return true;
    }
    return false;
  }
  if (name.rfind("std/collections/vector/", 0) == 0) {
    std::string alias = name.substr(std::string("std/collections/vector/").size());
    return false;
  }
  if (name.rfind("map/", 0) == 0) {
    std::string alias = name.substr(std::string("map/").size());
    if (alias == "map") {
      out = "map";
      return true;
    }
    return false;
  }
  if (name.rfind("std/collections/map/", 0) == 0) {
    std::string alias = name.substr(std::string("std/collections/map/").size());
    if (alias == "map") {
      out = "map";
      return true;
    }
    return false;
  }
  if (name.find('/') != std::string::npos) {
    return false;
  }
  if (name == "array" || name == "vector" || name == "map") {
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

std::string preferVectorStdlibHelperPath(const std::string &path,
                                         const std::unordered_map<std::string, std::string> &nameMap) {
  std::string preferred = path;
  if (!preferred.empty() && preferred.front() != '/' &&
      (preferred.rfind("map/", 0) == 0 || preferred.rfind("std/collections/map/", 0) == 0)) {
    preferred.insert(preferred.begin(), '/');
  }
  if (preferred.rfind("/array/", 0) == 0 && nameMap.count(preferred) == 0) {
    const std::string suffix = preferred.substr(std::string("/array/").size());
    if (allowsArrayVectorCompatibilitySuffix(suffix)) {
      const std::string vectorAlias = "/vector/" + suffix;
      if (nameMap.count(vectorAlias) > 0) {
        return vectorAlias;
      }
      const std::string stdlibAlias = "/std/collections/vector/" + suffix;
      if (nameMap.count(stdlibAlias) > 0) {
        return stdlibAlias;
      }
    }
  }
  if (preferred.rfind("/vector/", 0) == 0 && nameMap.count(preferred) == 0) {
    const std::string suffix = preferred.substr(std::string("/vector/").size());
    if (allowsVectorStdlibCompatibilitySuffix(suffix)) {
      const std::string stdlibAlias = "/std/collections/vector/" + suffix;
      if (nameMap.count(stdlibAlias) > 0) {
        preferred = stdlibAlias;
      } else {
        if (allowsArrayVectorCompatibilitySuffix(suffix)) {
          const std::string arrayAlias = "/array/" + suffix;
          if (nameMap.count(arrayAlias) > 0) {
            preferred = arrayAlias;
          }
        }
      }
    }
  }
  if (preferred.rfind("/std/collections/vector/", 0) == 0 && nameMap.count(preferred) == 0) {
    const std::string suffix = preferred.substr(std::string("/std/collections/vector/").size());
    if (allowsVectorStdlibCompatibilitySuffix(suffix)) {
      const std::string vectorAlias = "/vector/" + suffix;
      if (nameMap.count(vectorAlias) > 0) {
        preferred = vectorAlias;
      } else {
        if (allowsArrayVectorCompatibilitySuffix(suffix)) {
          const std::string arrayAlias = "/array/" + suffix;
          if (nameMap.count(arrayAlias) > 0) {
            preferred = arrayAlias;
          }
        }
      }
    }
  }
  if (preferred.rfind("/map/", 0) == 0 && nameMap.count(preferred) == 0) {
    const std::string stdlibAlias =
        "/std/collections/map/" + preferred.substr(std::string("/map/").size());
    if (nameMap.count(stdlibAlias) > 0) {
      preferred = stdlibAlias;
    }
  }
  if (preferred.rfind("/std/collections/map/", 0) == 0 && nameMap.count(preferred) == 0) {
    const std::string mapAlias =
        "/map/" + preferred.substr(std::string("/std/collections/map/").size());
    if (nameMap.count(mapAlias) > 0) {
      preferred = mapAlias;
    }
  }
  return preferred;
}

bool isArrayValue(const Expr &target, const std::unordered_map<std::string, BindingInfo> &localTypes) {
  if (target.kind == Expr::Kind::Name) {
    auto it = localTypes.find(target.name);
    if (it == localTypes.end()) {
      return false;
    }
    if (it->second.typeName == "array" || it->second.typeName == "vector") {
      return true;
    }
    if (it->second.typeName == "Reference") {
      std::string base;
      std::string arg;
      if (splitTemplateTypeName(it->second.typeTemplateArg, base, arg)) {
        return base == "array" || base == "vector";
      }
    }
    return false;
  }
  if (target.kind == Expr::Kind::Call) {
    std::string collection;
    return getBuiltinCollectionName(target, collection) && (collection == "array" || collection == "vector") &&
           target.templateArgs.size() == 1;
  }
  return false;
}

bool isExplicitArrayCountName(const Expr &expr) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  std::string normalized = expr.name;
  if (!normalized.empty() && normalized.front() == '/') {
    normalized.erase(normalized.begin());
  }
  return normalized == "array/count";
}

bool isVectorValue(const Expr &target, const std::unordered_map<std::string, BindingInfo> &localTypes) {
  if (target.kind == Expr::Kind::Name) {
    auto it = localTypes.find(target.name);
    if (it == localTypes.end()) {
      return false;
    }
    if (it->second.typeName == "vector") {
      return true;
    }
    if (it->second.typeName == "Reference") {
      std::string base;
      std::string arg;
      if (splitTemplateTypeName(it->second.typeTemplateArg, base, arg)) {
        return base == "vector";
      }
    }
    return false;
  }
  if (target.kind == Expr::Kind::Call) {
    std::string collection;
    return getBuiltinCollectionName(target, collection) && collection == "vector" && target.templateArgs.size() == 1;
  }
  return false;
}

bool isMapValue(const Expr &target, const std::unordered_map<std::string, BindingInfo> &localTypes) {
  if (target.kind == Expr::Kind::Name) {
    auto it = localTypes.find(target.name);
    if (it == localTypes.end()) {
      return false;
    }
    if (normalizeBindingTypeName(it->second.typeName) == "map") {
      return true;
    }
    if (normalizeBindingTypeName(it->second.typeName) == "Reference") {
      std::string base;
      std::string arg;
      if (splitTemplateTypeName(it->second.typeTemplateArg, base, arg)) {
        return normalizeBindingTypeName(base) == "map";
      }
    }
    return false;
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
    if ((isSimpleCallName(target, "at") || isSimpleCallName(target, "at_unsafe")) && target.args.size() == 2) {
      std::string elementType;
      if (inferCollectionElementTypeNameFromExpr(target.args.front(), localTypes, {}, elementType) &&
          normalizeBindingTypeName(elementType) == "string") {
        return true;
      }
    }
  }
  return false;
}

bool isArrayCountCall(const Expr &call, const std::unordered_map<std::string, BindingInfo> &localTypes) {
  if (!isSimpleCallName(call, "count") || call.args.size() != 1) {
    return false;
  }
  if (isExplicitArrayCountName(call) && isVectorValue(call.args.front(), localTypes)) {
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

bool isVectorCapacityCall(const Expr &call, const std::unordered_map<std::string, BindingInfo> &localTypes) {
  if (!isSimpleCallName(call, "capacity") || call.args.size() != 1) {
    return false;
  }
  return isVectorValue(call.args.front(), localTypes);
}

size_t getAccessCallReceiverIndex(const Expr &call,
                                  const std::unordered_map<std::string, BindingInfo> &localTypes) {
  if (call.args.size() != 2) {
    return 0;
  }
  if (hasNamedArguments(call.argNames)) {
    for (size_t i = 0; i < call.argNames.size() && i < call.args.size(); ++i) {
      if (call.argNames[i].has_value() && *call.argNames[i] == "values") {
        return i;
      }
    }
  }
  const bool leadingIsCollectionLike = isArrayValue(call.args.front(), localTypes) ||
                                       isVectorValue(call.args.front(), localTypes) ||
                                       isMapValue(call.args.front(), localTypes) ||
                                       isStringValue(call.args.front(), localTypes);
  const bool trailingIsCollectionLike = isArrayValue(call.args[1], localTypes) || isVectorValue(call.args[1], localTypes) ||
                                        isMapValue(call.args[1], localTypes) || isStringValue(call.args[1], localTypes);
  if (!leadingIsCollectionLike && trailingIsCollectionLike) {
    return 1;
  }
  return 0;
}

bool inferCollectionElementTypeNameFromBinding(const BindingInfo &binding, std::string &typeOut) {
  std::string typeName = normalizeBindingTypeName(binding.typeName);
  std::string templateArg = binding.typeTemplateArg;
  if (typeName == "Reference" && !templateArg.empty()) {
    std::string base;
    std::string arg;
    if (splitTemplateTypeName(templateArg, base, arg)) {
      typeName = normalizeBindingTypeName(base);
      templateArg = arg;
    } else {
      typeName = normalizeBindingTypeName(templateArg);
      templateArg.clear();
    }
  }
  if ((typeName == "array" || typeName == "vector") && !templateArg.empty()) {
    typeOut = normalizeBindingTypeName(templateArg);
    return true;
  }
  if (typeName == "map" && !templateArg.empty()) {
    std::vector<std::string> templateParts;
    if (splitTopLevelTemplateArgs(templateArg, templateParts) && templateParts.size() == 2) {
      typeOut = normalizeBindingTypeName(templateParts[1]);
      return true;
    }
  }
  return false;
}

bool inferCollectionElementTypeNameFromExpr(const Expr &expr,
                                            const std::unordered_map<std::string, BindingInfo> &localTypes,
                                            const std::function<bool(const Expr &, std::string &)> &resolveCallElementTypeName,
                                            std::string &typeOut) {
  if (expr.kind == Expr::Kind::Name) {
    auto it = localTypes.find(expr.name);
    if (it == localTypes.end()) {
      return false;
    }
    return inferCollectionElementTypeNameFromBinding(it->second, typeOut);
  }
  if (expr.kind == Expr::Kind::Call) {
    if (resolveCallElementTypeName && resolveCallElementTypeName(expr, typeOut)) {
      return true;
    }
    std::string collectionName;
    if (!getBuiltinCollectionName(expr, collectionName)) {
      return false;
    }
    if ((collectionName == "array" || collectionName == "vector") && expr.templateArgs.size() == 1) {
      typeOut = normalizeBindingTypeName(expr.templateArgs.front());
      return true;
    }
    if (collectionName == "map" && expr.templateArgs.size() == 2) {
      typeOut = normalizeBindingTypeName(expr.templateArgs[1]);
      return true;
    }
  }
  return false;
}

std::string inferAccessCallTypeName(const Expr &call,
                                    const std::unordered_map<std::string, BindingInfo> &localTypes,
                                    const std::function<std::string(const Expr &)> &inferPrimitiveTypeName,
                                    const std::function<bool(const Expr &, std::string &)> &resolveCallElementTypeName) {
  std::string accessName;
  if (!getBuiltinArrayAccessNameLocal(call, accessName) || call.args.size() != 2) {
    return "";
  }
  const size_t receiverIndex = getAccessCallReceiverIndex(call, localTypes);
  if (receiverIndex >= call.args.size()) {
    return "";
  }
  const Expr &receiver = call.args[receiverIndex];
  if (isStringValue(receiver, localTypes)) {
    return "i32";
  }
  if (inferPrimitiveTypeName) {
    const std::string inferredReceiverType = normalizeBindingTypeName(inferPrimitiveTypeName(receiver));
    if (inferredReceiverType == "string") {
      return "i32";
    }
  }
  std::string elementType;
  if (inferCollectionElementTypeNameFromExpr(receiver, localTypes, resolveCallElementTypeName, elementType)) {
    return elementType;
  }
  return "";
}

bool resolveMethodCallPath(const Expr &call,
                           const std::unordered_map<std::string, const Definition *> &defMap,
                           const std::unordered_map<std::string, BindingInfo> &localTypes,
                           const std::unordered_map<std::string, std::string> &importAliases,
                           const std::unordered_map<std::string, std::string> &structTypeMap,
                           const std::unordered_map<std::string, ReturnKind> &returnKinds,
                           const std::unordered_map<std::string, std::string> &returnStructs,
                           std::string &resolvedOut) {
  if (!call.isMethodCall || call.args.empty()) {
    return false;
  }
  auto isRemovedVectorCompatibilityHelper = [](std::string_view helperName) {
    return helperName == "count" || helperName == "capacity" || helperName == "at" || helperName == "at_unsafe" ||
           helperName == "push" || helperName == "pop" || helperName == "reserve" || helperName == "clear" ||
           helperName == "remove_at" || helperName == "remove_swap";
  };
  auto isRemovedMapCompatibilityHelper = [](std::string_view helperName) {
    return helperName == "count" || helperName == "at" || helperName == "at_unsafe";
  };
  auto isRemovedCollectionMethodAlias = [&](const std::string &rawMethodName) {
    std::string candidate = rawMethodName;
    if (!candidate.empty() && candidate.front() == '/') {
      candidate.erase(candidate.begin());
    }
    std::string_view helperName;
    if (candidate.rfind("array/", 0) == 0) {
      helperName = std::string_view(candidate).substr(std::string_view("array/").size());
      return isRemovedVectorCompatibilityHelper(helperName);
    }
    if (candidate.rfind("vector/", 0) == 0) {
      helperName = std::string_view(candidate).substr(std::string_view("vector/").size());
      return isRemovedVectorCompatibilityHelper(helperName);
    }
    if (candidate.rfind("std/collections/vector/", 0) == 0) {
      helperName = std::string_view(candidate).substr(std::string_view("std/collections/vector/").size());
      return isRemovedVectorCompatibilityHelper(helperName);
    }
    if (candidate.rfind("map/", 0) == 0) {
      helperName = std::string_view(candidate).substr(std::string_view("map/").size());
      return isRemovedMapCompatibilityHelper(helperName);
    }
    if (candidate.rfind("std/collections/map/", 0) == 0) {
      helperName = std::string_view(candidate).substr(std::string_view("std/collections/map/").size());
      return isRemovedMapCompatibilityHelper(helperName);
    }
    return false;
  };
  if (isRemovedCollectionMethodAlias(call.name)) {
    resolvedOut.clear();
    return false;
  }
  std::string normalizedMethodName = call.name;
  if (!normalizedMethodName.empty() && normalizedMethodName.front() == '/') {
    normalizedMethodName.erase(normalizedMethodName.begin());
  }
  if (normalizedMethodName.rfind("vector/", 0) == 0) {
    normalizedMethodName = normalizedMethodName.substr(std::string("vector/").size());
  } else if (normalizedMethodName.rfind("array/", 0) == 0) {
    normalizedMethodName = normalizedMethodName.substr(std::string("array/").size());
  } else if (normalizedMethodName.rfind("std/collections/vector/", 0) == 0) {
    normalizedMethodName = normalizedMethodName.substr(std::string("std/collections/vector/").size());
  } else if (normalizedMethodName.rfind("map/", 0) == 0) {
    normalizedMethodName = normalizedMethodName.substr(std::string("map/").size());
  } else if (normalizedMethodName.rfind("std/collections/map/", 0) == 0) {
    normalizedMethodName = normalizedMethodName.substr(std::string("std/collections/map/").size());
  }
  const Expr &receiver = call.args.front();
  auto normalizeCollectionReceiverType = [](const std::string &typePath) -> std::string {
    if (typePath == "/array" || typePath == "array") {
      return "array";
    }
    if (typePath == "/vector" || typePath == "vector") {
      return "vector";
    }
    if (typePath == "/map" || typePath == "map") {
      return "map";
    }
    return typePath;
  };
  auto collectionHelperPathCandidates = [](const std::string &path) {
    std::vector<std::string> candidates;
    auto appendUnique = [&](const std::string &candidate) {
      if (candidate.empty()) {
        return;
      }
      for (const auto &existing : candidates) {
        if (existing == candidate) {
          return;
        }
      }
      candidates.push_back(candidate);
    };

    std::string normalizedPath = path;
    if (!normalizedPath.empty() && normalizedPath.front() != '/') {
      if (normalizedPath.rfind("array/", 0) == 0 || normalizedPath.rfind("vector/", 0) == 0 ||
          normalizedPath.rfind("std/collections/vector/", 0) == 0 || normalizedPath.rfind("map/", 0) == 0 ||
          normalizedPath.rfind("std/collections/map/", 0) == 0) {
        normalizedPath.insert(normalizedPath.begin(), '/');
      }
    }

    appendUnique(path);
    appendUnique(normalizedPath);
    if (normalizedPath.rfind("/array/", 0) == 0) {
      const std::string suffix = normalizedPath.substr(std::string("/array/").size());
      if (allowsArrayVectorCompatibilitySuffix(suffix)) {
        appendUnique("/vector/" + suffix);
        appendUnique("/std/collections/vector/" + suffix);
      }
    } else if (normalizedPath.rfind("/vector/", 0) == 0) {
      const std::string suffix = normalizedPath.substr(std::string("/vector/").size());
      if (allowsVectorStdlibCompatibilitySuffix(suffix)) {
        appendUnique("/std/collections/vector/" + suffix);
      }
      if (allowsArrayVectorCompatibilitySuffix(suffix)) {
        appendUnique("/array/" + suffix);
      }
    } else if (normalizedPath.rfind("/std/collections/vector/", 0) == 0) {
      const std::string suffix = normalizedPath.substr(std::string("/std/collections/vector/").size());
      if (allowsVectorStdlibCompatibilitySuffix(suffix)) {
        appendUnique("/vector/" + suffix);
      }
      if (allowsArrayVectorCompatibilitySuffix(suffix)) {
        appendUnique("/array/" + suffix);
      }
    } else if (normalizedPath.rfind("/map/", 0) == 0) {
      appendUnique("/std/collections/map/" + normalizedPath.substr(std::string("/map/").size()));
    } else if (normalizedPath.rfind("/std/collections/map/", 0) == 0) {
      const std::string suffix = normalizedPath.substr(std::string("/std/collections/map/").size());
      const std::string aliasPath = "/map/" + suffix;
      appendUnique(aliasPath);
    }
    return candidates;
  };
  auto pruneMapAccessStructReturnCompatibilityCandidates = [](const std::string &path,
                                                              std::vector<std::string> &candidates) {
    std::string normalizedPath = path;
    if (!normalizedPath.empty() && normalizedPath.front() != '/') {
      if (normalizedPath.rfind("map/", 0) == 0 || normalizedPath.rfind("std/collections/map/", 0) == 0) {
        normalizedPath.insert(normalizedPath.begin(), '/');
      }
    }
    auto eraseCandidate = [&](const std::string &candidate) {
      for (auto it = candidates.begin(); it != candidates.end();) {
        if (*it == candidate) {
          it = candidates.erase(it);
        } else {
          ++it;
        }
      }
    };
    if (normalizedPath.rfind("/map/", 0) == 0) {
      const std::string suffix = normalizedPath.substr(std::string("/map/").size());
      if (suffix == "at" || suffix == "at_unsafe") {
        eraseCandidate("/std/collections/map/" + suffix);
      }
    } else if (normalizedPath.rfind("/std/collections/map/", 0) == 0) {
      const std::string suffix = normalizedPath.substr(std::string("/std/collections/map/").size());
      if (suffix == "at" || suffix == "at_unsafe") {
        eraseCandidate("/map/" + suffix);
      }
    }
  };
  auto normalizeMapImportAliasPath = [](const std::string &path) {
    if (path.empty() || path.front() == '/') {
      return path;
    }
    if (path.rfind("map/", 0) == 0 || path.rfind("std/collections/map/", 0) == 0) {
      return "/" + path;
    }
    return path;
  };
  auto metadataPathCandidates = [&](const std::string &path) {
    std::vector<std::string> candidates;
    auto appendUnique = [&](const std::string &candidate) {
      if (candidate.empty()) {
        return;
      }
      for (const auto &existing : candidates) {
        if (existing == candidate) {
          return;
        }
      }
      candidates.push_back(candidate);
    };
    appendUnique(path);
    appendUnique(normalizeMapImportAliasPath(path));
    if (!path.empty() && path.front() == '/' &&
        (path.rfind("/map/", 0) == 0 || path.rfind("/std/collections/map/", 0) == 0)) {
      appendUnique(path.substr(1));
    }
    return candidates;
  };
  auto findStructTypeMetadata = [&](const std::string &path) -> const std::string * {
    for (const auto &candidate : metadataPathCandidates(path)) {
      auto it = structTypeMap.find(candidate);
      if (it != structTypeMap.end()) {
        return &it->second;
      }
    }
    return nullptr;
  };
  auto findReturnStructMetadata = [&](const std::string &path) -> const std::string * {
    for (const auto &candidate : metadataPathCandidates(path)) {
      auto it = returnStructs.find(candidate);
      if (it != returnStructs.end()) {
        return &it->second;
      }
    }
    return nullptr;
  };
  auto findReturnKindMetadata = [&](const std::string &path) -> const ReturnKind * {
    for (const auto &candidate : metadataPathCandidates(path)) {
      auto it = returnKinds.find(candidate);
      if (it != returnKinds.end()) {
        return &it->second;
      }
    }
    return nullptr;
  };
  auto preferCanonicalMapMethodHelperPath = [&](const std::string &path) {
    if (path.rfind("/map/", 0) != 0) {
      return path;
    }
    const std::string suffix = path.substr(std::string("/map/").size());
    if (suffix != "count" && suffix != "at" && suffix != "at_unsafe") {
      return path;
    }
    const std::string canonicalPath = "/std/collections/map/" + suffix;
    if (defMap.count(canonicalPath) > 0 || findStructTypeMetadata(canonicalPath) != nullptr ||
        findReturnStructMetadata(canonicalPath) != nullptr || findReturnKindMetadata(canonicalPath) != nullptr) {
      return canonicalPath;
    }
    return path;
  };
  std::function<std::string(const Expr &)> inferPrimitiveTypeName;
  auto resolveCollectionElementTypeFromCall = [&](const Expr &candidate, std::string &typeOut) -> bool {
    typeOut.clear();
    if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall) {
      return false;
    }

    auto extractCollectionElementTypeFromReturn = [&](const std::string &typeName) -> bool {
      std::string normalizedType = normalizeBindingTypeName(typeName);
      while (true) {
        std::string base;
        std::string arg;
        if (!splitTemplateTypeName(normalizedType, base, arg)) {
          return false;
        }
        std::vector<std::string> args;
        if (!splitTopLevelTemplateArgs(arg, args)) {
          return false;
        }
        if ((base == "array" || base == "vector") && args.size() == 1) {
          typeOut = normalizeBindingTypeName(args.front());
          return true;
        }
        if (base == "map" && args.size() == 2) {
          typeOut = normalizeBindingTypeName(args[1]);
          return true;
        }
        if ((base == "Reference" || base == "Pointer") && args.size() == 1) {
          normalizedType = normalizeBindingTypeName(args.front());
          continue;
        }
        return false;
      }
    };

    const std::string resolvedExprPath = resolveExprPath(candidate);
    std::vector<std::string> resolvedCandidates = collectionHelperPathCandidates(resolvedExprPath);
    pruneMapAccessStructReturnCompatibilityCandidates(resolvedExprPath, resolvedCandidates);
    auto importIt = importAliases.find(candidate.name);
    if (importIt != importAliases.end()) {
      for (const auto &aliasCandidate : collectionHelperPathCandidates(importIt->second)) {
        bool seen = false;
        for (const auto &existing : resolvedCandidates) {
          if (existing == aliasCandidate) {
            seen = true;
            break;
          }
        }
        if (!seen) {
          resolvedCandidates.push_back(aliasCandidate);
        }
      }
    }

    for (const auto &path : resolvedCandidates) {
      auto defIt = defMap.find(path);
      if (defIt == defMap.end() || defIt->second == nullptr) {
        continue;
      }
      for (const auto &transform : defIt->second->transforms) {
        if (transform.name != "return" || transform.templateArgs.size() != 1) {
          continue;
        }
        return extractCollectionElementTypeFromReturn(transform.templateArgs.front());
      }
    }
    return false;
  };
  auto isExplicitMapAccessCompatibilityCall = [&](const Expr &candidate) {
    if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall || candidate.name.empty()) {
      return false;
    }
    std::string normalized = candidate.name;
    if (!normalized.empty() && normalized.front() == '/') {
      normalized.erase(normalized.begin());
    }
    if (normalized != "map/at" && normalized != "map/at_unsafe") {
      return false;
    }
    if (defMap.find("/" + normalized) != defMap.end()) {
      return false;
    }
    const size_t receiverIndex = getAccessCallReceiverIndex(candidate, localTypes);
    return receiverIndex < candidate.args.size() && isMapValue(candidate.args[receiverIndex], localTypes);
  };
  auto isExplicitVectorAccessCompatibilityCall = [&](const Expr &candidate) {
    if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall || candidate.name.empty()) {
      return false;
    }
    std::string normalized = candidate.name;
    if (!normalized.empty() && normalized.front() == '/') {
      normalized.erase(normalized.begin());
    }
    if (normalized != "vector/at" && normalized != "vector/at_unsafe") {
      return false;
    }
    if (defMap.find("/" + normalized) != defMap.end()) {
      return false;
    }
    const size_t receiverIndex = getAccessCallReceiverIndex(candidate, localTypes);
    if (receiverIndex >= candidate.args.size()) {
      return false;
    }
    return isArrayValue(candidate.args[receiverIndex], localTypes) || isVectorValue(candidate.args[receiverIndex], localTypes) ||
           isStringValue(candidate.args[receiverIndex], localTypes);
  };
  auto inferExplicitMapAccessCompatibilityTypeName = [&](const Expr &candidate) -> std::string {
    if (!isExplicitMapAccessCompatibilityCall(candidate)) {
      return "";
    }
    const size_t receiverIndex = getAccessCallReceiverIndex(candidate, localTypes);
    if (receiverIndex >= candidate.args.size()) {
      return "";
    }
    std::string elementType;
    if (inferCollectionElementTypeNameFromExpr(
            candidate.args[receiverIndex], localTypes, resolveCollectionElementTypeFromCall, elementType)) {
      return normalizeBindingTypeName(elementType);
    }
    return "";
  };
  auto inferExplicitVectorAccessCompatibilityTypeName = [&](const Expr &candidate) -> std::string {
    if (!isExplicitVectorAccessCompatibilityCall(candidate)) {
      return "";
    }
    const size_t receiverIndex = getAccessCallReceiverIndex(candidate, localTypes);
    if (receiverIndex >= candidate.args.size()) {
      return "";
    }
    const Expr &receiver = candidate.args[receiverIndex];
    if (isStringValue(receiver, localTypes)) {
      return "i32";
    }
    if (inferPrimitiveTypeName) {
      const std::string inferredReceiverType = normalizeBindingTypeName(inferPrimitiveTypeName(receiver));
      if (inferredReceiverType == "string") {
        return "i32";
      }
    }
    std::string elementType;
    if (inferCollectionElementTypeNameFromExpr(receiver, localTypes, resolveCollectionElementTypeFromCall, elementType)) {
      return normalizeBindingTypeName(elementType);
    }
    return "";
  };
  auto inferCanonicalMapAccessTypeName = [&](const Expr &candidate) -> std::string {
    if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall || candidate.name.empty()) {
      return "";
    }
    std::string normalized = candidate.name;
    if (!normalized.empty() && normalized.front() == '/') {
      normalized.erase(normalized.begin());
    }
    if (normalized != "std/collections/map/at" && normalized != "std/collections/map/at_unsafe") {
      return "";
    }
    const size_t receiverIndex = getAccessCallReceiverIndex(candidate, localTypes);
    if (receiverIndex >= candidate.args.size()) {
      return "";
    }
    std::string elementType;
    if (inferCollectionElementTypeNameFromExpr(
            candidate.args[receiverIndex], localTypes, resolveCollectionElementTypeFromCall, elementType)) {
      return normalizeBindingTypeName(elementType);
    }
    return "";
  };
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
        if (!expr.isMethodCall) {
          if (const std::string explicitVectorAccessType = inferExplicitVectorAccessCompatibilityTypeName(expr);
              !explicitVectorAccessType.empty()) {
            return explicitVectorAccessType;
          }
          if (const std::string explicitMapAccessType = inferExplicitMapAccessCompatibilityTypeName(expr);
              !explicitMapAccessType.empty()) {
            return explicitMapAccessType;
          }
          if (const std::string canonicalMapAccessType = inferCanonicalMapAccessTypeName(expr);
              !canonicalMapAccessType.empty()) {
            return canonicalMapAccessType;
          }
          const std::string resolvedExprPath = resolveExprPath(expr);
          std::vector<std::string> resolvedCandidates = collectionHelperPathCandidates(resolvedExprPath);
          pruneMapAccessStructReturnCompatibilityCandidates(resolvedExprPath, resolvedCandidates);
          auto importIt = importAliases.find(expr.name);
          if (importIt != importAliases.end()) {
            for (const auto &candidate : collectionHelperPathCandidates(importIt->second)) {
              bool seen = false;
              for (const auto &existing : resolvedCandidates) {
                if (existing == candidate) {
                  seen = true;
                  break;
                }
              }
              if (!seen) {
                resolvedCandidates.push_back(candidate);
              }
            }
          }
          for (const auto &candidate : resolvedCandidates) {
            if (const std::string *structPath = findReturnStructMetadata(candidate)) {
              return normalizeCollectionReceiverType(*structPath);
            }
          }
          for (const auto &candidate : resolvedCandidates) {
            const ReturnKind *kind = findReturnKindMetadata(candidate);
            if (kind == nullptr) {
              continue;
            }
            if (*kind == ReturnKind::Array) {
              return "array";
            }
            const std::string inferredType = typeNameForReturnKind(*kind);
            if (!inferredType.empty()) {
              return inferredType;
            }
          }
        }
        std::string collectionName;
        if (getBuiltinCollectionName(expr, collectionName)) {
          if ((collectionName == "array" || collectionName == "vector") && expr.templateArgs.size() == 1) {
            return collectionName;
          }
          if (collectionName == "map" && expr.templateArgs.size() == 2) {
            return collectionName;
          }
        }
        if (isArrayCountCall(expr, localTypes)) {
          return "i32";
        }
        if (isMapCountCall(expr, localTypes)) {
          return "i32";
        }
        if (isStringCountCall(expr, localTypes)) {
          return "i32";
        }
        if (isVectorCapacityCall(expr, localTypes)) {
          return "i32";
        }
        const std::string accessTypeName =
            inferAccessCallTypeName(expr, localTypes, inferPrimitiveTypeName, resolveCollectionElementTypeFromCall);
        if (!accessTypeName.empty()) {
          return accessTypeName;
        }
        if (!expr.isMethodCall) {
          return "";
        }
        std::string methodPath;
        if (resolveMethodCallPath(
                expr, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, methodPath)) {
          if (const std::string *structPath = findReturnStructMetadata(methodPath)) {
            return normalizeCollectionReceiverType(*structPath);
          }
          if (const ReturnKind *kind = findReturnKindMetadata(methodPath)) {
            if (*kind == ReturnKind::Array) {
              return "array";
            }
            return typeNameForReturnKind(*kind);
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
  } else if (receiver.kind == Expr::Kind::Call && !receiver.isMethodCall && !receiver.isBinding) {
    std::string resolved = resolveExprPath(receiver);
    auto normalizeResolvedPath = [](std::string path) {
      if (!path.empty() && path.front() != '/') {
        path.insert(path.begin(), '/');
      }
      return path;
    };
    auto hasStructPath = [&](const std::string &path) { return findStructTypeMetadata(path) != nullptr; };
    auto importIt = importAliases.find(receiver.name);
    if (!hasStructPath(resolved) && importIt != importAliases.end()) {
      resolved = importIt->second;
    }
    const std::string normalizedResolved = normalizeResolvedPath(resolved);
    if (!hasStructPath(resolved) && hasStructPath(normalizedResolved)) {
      resolved = normalizedResolved;
    }
    if (hasStructPath(resolved)) {
      resolved = normalizeResolvedPath(resolved);
      resolvedOut = resolved + "/" + normalizedMethodName;
      return true;
    }
    typeName = inferPrimitiveTypeName(receiver);
  } else {
    typeName = inferPrimitiveTypeName(receiver);
  }
  if (typeName.empty()) {
    return false;
  }
  if (typeName == "File") {
    resolvedOut = "/file/" + normalizedMethodName;
    return true;
  }
  if (typeName == "FileError" && normalizedMethodName == "why") {
    resolvedOut = "/file_error/why";
    return true;
  }
  if (typeName == "Pointer" || typeName == "Reference") {
    return false;
  }
  if (isPrimitiveBindingTypeName(typeName)) {
    resolvedOut = "/" + normalizeBindingTypeName(typeName) + "/" + normalizedMethodName;
    return true;
  }
  typeName = normalizeMapImportAliasPath(typeName);
  std::string resolvedType = resolveTypePath(typeName, call.namespacePrefix);
  if (findReturnKindMetadata(resolvedType) == nullptr) {
    auto importIt = importAliases.find(typeName);
    if (importIt != importAliases.end()) {
      resolvedType = normalizeMapImportAliasPath(importIt->second);
    }
  }
  resolvedOut = preferCanonicalMapMethodHelperPath(resolvedType + "/" + normalizedMethodName);
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

bool getVectorMutatorName(const Expr &expr,
                          const std::unordered_map<std::string, std::string> &nameMap,
                          std::string &out) {
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
  if (name.rfind("vector/", 0) == 0) {
    name = name.substr(std::string("vector/").size());
  } else if (name.rfind("std/collections/vector/", 0) == 0) {
    name = name.substr(std::string("std/collections/vector/").size());
  } else if (name.find('/') != std::string::npos) {
    return false;
  }
  if (name.find('/') != std::string::npos) {
    return false;
  }
  if (name == "push" || name == "pop" || name == "reserve" || name == "clear" || name == "remove_at" ||
      name == "remove_swap") {
    out = name;
    return true;
  }
  return false;
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
  auto isCollectionBindingType = [](const std::string &typeName) {
    const std::string normalizedTypeName = normalizeBindingTypeName(typeName);
    return normalizedTypeName == "array" || normalizedTypeName == "vector" || normalizedTypeName == "map" ||
           normalizedTypeName == "string" || normalizedTypeName == "soa_vector";
  };
  if (!hasNamedArguments(expr.argNames) && expr.args.size() == 2 && params.size() == 2 &&
      params[0].name == "values" && isCollectionBindingType(getBindingInfo(params[0]).typeName)) {
    const Expr &first = expr.args.front();
    const bool leadingNonReceiver =
        first.kind == Expr::Kind::Literal || first.kind == Expr::Kind::BoolLiteral ||
        first.kind == Expr::Kind::FloatLiteral || first.kind == Expr::Kind::StringLiteral;
    if (leadingNonReceiver) {
      ordered.assign(2, nullptr);
      ordered[0] = &expr.args[1];
      ordered[1] = &expr.args[0];
      return ordered;
    }
  }
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

bool isLoopCall(const Expr &expr) {
  return isSimpleCallName(expr, "loop");
}

bool isWhileCall(const Expr &expr) {
  return isSimpleCallName(expr, "while");
}

bool isForCall(const Expr &expr) {
  return isSimpleCallName(expr, "for");
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
