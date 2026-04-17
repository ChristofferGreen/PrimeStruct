#include "EmitterBuiltinCallPathHelpersInternal.h"
#include "EmitterHelpers.h"
#include "primec/StringLiteral.h"

namespace primec::emitter {

std::string preferredFileMethodTargetLocal(
    const std::string &methodName,
    const std::unordered_map<std::string, const Definition *> &defMap) {
  const std::string stdlibPath = "/File/" + methodName;
  if (defMap.find(stdlibPath) != defMap.end()) {
    return stdlibPath;
  }
  return "/file/" + methodName;
}

bool isMapCollectionTypeNameLocal(const std::string &name) {
  const std::string normalized = normalizeBindingTypeName(name);
  return normalized == "map" || normalized == "std/collections/map" || normalized == "Map" ||
         normalized == "std/collections/experimental_map/Map";
}

bool extractMapKeyValueTypesFromTypeTextLocal(const std::string &typeText,
                                              std::string &keyTypeOut,
                                              std::string &valueTypeOut) {
  keyTypeOut.clear();
  valueTypeOut.clear();
  std::string normalizedType = normalizeBindingTypeName(typeText);
  while (true) {
    std::string base;
    std::string argText;
    if (!splitTemplateTypeName(normalizedType, base, argText)) {
      return false;
    }
    base = normalizeBindingTypeName(base);
    if (isMapCollectionTypeNameLocal(base)) {
      std::vector<std::string> parts;
      if (!splitTopLevelTemplateArgs(argText, parts) || parts.size() != 2) {
        return false;
      }
      keyTypeOut = parts[0];
      valueTypeOut = parts[1];
      return true;
    }
    if (base == "Reference" || base == "Pointer") {
      std::vector<std::string> args;
      if (!splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
        return false;
      }
      normalizedType = normalizeBindingTypeName(args.front());
      continue;
    }
    return false;
  }
}

bool extractMapKeyValueTypesLocal(const BindingInfo &binding,
                                  std::string &keyTypeOut,
                                  std::string &valueTypeOut) {
  if (binding.typeTemplateArg.empty()) {
    return extractMapKeyValueTypesFromTypeTextLocal(binding.typeName, keyTypeOut, valueTypeOut);
  }
  return extractMapKeyValueTypesFromTypeTextLocal(binding.typeName + "<" + binding.typeTemplateArg + ">",
                                                  keyTypeOut,
                                                  valueTypeOut);
}

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
  if (name.rfind("std/collections/vector/", 0) == 0) {
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
      const std::string stdlibAlias = "/std/collections/vector/" + suffix;
      if (nameMap.count(stdlibAlias) > 0) {
        return stdlibAlias;
      }
    }
  }
  if (preferred.rfind("/map/", 0) == 0 && nameMap.count(preferred) == 0) {
    const std::string suffix = preferred.substr(std::string("/map/").size());
    if (suffix != "count" && suffix != "contains" && suffix != "tryAt" &&
        suffix != "at" && suffix != "at_unsafe") {
      const std::string stdlibAlias = "/std/collections/map/" + suffix;
      if (nameMap.count(stdlibAlias) > 0) {
        preferred = stdlibAlias;
      }
    }
  }
  if (preferred.rfind("/std/collections/map/", 0) == 0 && nameMap.count(preferred) == 0) {
    const std::string suffix = preferred.substr(std::string("/std/collections/map/").size());
    if (suffix != "count" && suffix != "contains" && suffix != "tryAt" &&
        suffix != "at" && suffix != "at_unsafe") {
      const std::string mapAlias = "/map/" + suffix;
      if (nameMap.count(mapAlias) > 0) {
        preferred = mapAlias;
      }
    }
  }
  return preferred;
}

} // namespace primec::emitter
