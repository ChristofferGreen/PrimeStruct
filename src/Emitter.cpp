#include "primec/Emitter.h"
#include "primec/StringLiteral.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace primec {
namespace {
using BindingInfo = Emitter::BindingInfo;
using ReturnKind = Emitter::ReturnKind;

ReturnKind getReturnKind(const Definition &def) {
  for (const auto &transform : def.transforms) {
    if (transform.name == "struct" || transform.name == "pod" || transform.name == "stack" ||
        transform.name == "heap" || transform.name == "buffer") {
      return ReturnKind::Void;
    }
    if (transform.name != "return" || !transform.templateArg) {
      continue;
    }
    if (*transform.templateArg == "void") {
      return ReturnKind::Void;
    }
  if (*transform.templateArg == "int") {
    return ReturnKind::Int;
  }
  if (*transform.templateArg == "bool") {
    return ReturnKind::Bool;
  }
  if (*transform.templateArg == "i32") {
    return ReturnKind::Int;
  }
  if (*transform.templateArg == "i64") {
    return ReturnKind::Int64;
  }
  if (*transform.templateArg == "u64") {
    return ReturnKind::UInt64;
  }
  if (*transform.templateArg == "float" || *transform.templateArg == "f32") {
    return ReturnKind::Float32;
  }
  if (*transform.templateArg == "f64") {
    return ReturnKind::Float64;
    }
  }
  return ReturnKind::Unknown;
}

bool isPrimitiveBindingTypeName(const std::string &name) {
  return name == "int" || name == "i32" || name == "i64" || name == "u64" || name == "float" || name == "f32" ||
         name == "f64" || name == "bool" || name == "string";
}

std::string normalizeBindingTypeName(const std::string &name) {
  if (name == "int") {
    return "i32";
  }
  if (name == "float") {
    return "f32";
  }
  return name;
}

bool isBindingQualifierName(const std::string &name) {
  return name == "public" || name == "private" || name == "package" || name == "static";
}

bool isBindingAuxTransformName(const std::string &name) {
  return name == "mut" || name == "copy" || name == "restrict" || name == "align_bytes" || name == "align_kbytes" ||
         isBindingQualifierName(name);
}

bool hasExplicitBindingTypeTransform(const Expr &expr) {
  for (const auto &transform : expr.transforms) {
    if (transform.name == "effects" || transform.name == "capabilities") {
      continue;
    }
    if (isBindingAuxTransformName(transform.name)) {
      continue;
    }
    if (!transform.arguments.empty()) {
      continue;
    }
    return true;
  }
  return false;
}

BindingInfo getBindingInfo(const Expr &expr) {
  BindingInfo info;
  for (const auto &transform : expr.transforms) {
    if (transform.name == "mut" && !transform.templateArg && transform.arguments.empty()) {
      info.isMutable = true;
      continue;
    }
    if (transform.name == "copy" || transform.name == "restrict" || transform.name == "align_bytes" ||
        transform.name == "align_kbytes") {
      continue;
    }
    if (transform.arguments.empty()) {
      if (isBindingQualifierName(transform.name)) {
        continue;
      }
      if (!transform.templateArg) {
        info.typeName = transform.name;
      } else if (info.typeName.empty()) {
        info.typeName = transform.name;
        info.typeTemplateArg = *transform.templateArg;
      }
    }
  }
  if (info.typeName.empty()) {
    info.typeName = "int";
  }
  return info;
}

std::string bindingTypeToCpp(const BindingInfo &info);

std::string bindingTypeToCpp(const std::string &typeName) {
  if (typeName == "i32" || typeName == "int") {
    return "int";
  }
  if (typeName == "i64") {
    return "int64_t";
  }
  if (typeName == "u64") {
    return "uint64_t";
  }
  if (typeName == "bool") {
    return "bool";
  }
  if (typeName == "float" || typeName == "f32") {
    return "float";
  }
  if (typeName == "f64") {
    return "double";
  }
  if (typeName == "string") {
    return "std::string_view";
  }
  return "int";
}

std::string bindingTypeToCpp(const BindingInfo &info) {
  if (info.typeName == "array") {
    std::string elemType = bindingTypeToCpp(info.typeTemplateArg);
    return "std::vector<" + elemType + ">";
  }
  if (info.typeName == "Pointer") {
    std::string base = bindingTypeToCpp(info.typeTemplateArg);
    if (base.empty()) {
      base = "void";
    }
    return base + " *";
  }
  if (info.typeName == "Reference") {
    std::string base = bindingTypeToCpp(info.typeTemplateArg);
    if (base.empty()) {
      base = "void";
    }
    return base + " &";
  }
  return bindingTypeToCpp(info.typeName);
}

std::string resolveTypePath(const std::string &name, const std::string &namespacePrefix) {
  if (!name.empty() && name[0] == '/') {
    return name;
  }
  if (!namespacePrefix.empty()) {
    return namespacePrefix + "/" + name;
  }
  return "/" + name;
}

std::string typeNameForReturnKind(ReturnKind kind) {
  switch (kind) {
    case ReturnKind::Int:
      return "i32";
    case ReturnKind::Int64:
      return "i64";
    case ReturnKind::UInt64:
      return "u64";
    case ReturnKind::Bool:
      return "bool";
    case ReturnKind::Float32:
      return "f32";
    case ReturnKind::Float64:
      return "f64";
    default:
      return "";
  }
}

ReturnKind returnKindForTypeName(const std::string &name) {
  if (name == "int" || name == "i32") {
    return ReturnKind::Int;
  }
  if (name == "i64") {
    return ReturnKind::Int64;
  }
  if (name == "u64") {
    return ReturnKind::UInt64;
  }
  if (name == "bool") {
    return ReturnKind::Bool;
  }
  if (name == "float" || name == "f32") {
    return ReturnKind::Float32;
  }
  if (name == "f64") {
    return ReturnKind::Float64;
  }
  if (name == "void") {
    return ReturnKind::Void;
  }
  return ReturnKind::Unknown;
}

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
  if (typeName == "Pointer" || typeName == "Reference" || typeName == "array" || typeName == "map") {
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

bool isRepeatCall(const Expr &expr) {
  return isSimpleCallName(expr, "repeat");
}

bool isReturnCall(const Expr &expr) {
  return isSimpleCallName(expr, "return");
}
} // namespace

std::string Emitter::toCppName(const std::string &fullPath) const {
  std::string name = "ps";
  for (char c : fullPath) {
    if (c == '/') {
      name += "_";
    } else {
      name += c;
    }
  }
  return name;
}

std::string Emitter::emitExpr(const Expr &expr,
                              const std::unordered_map<std::string, std::string> &nameMap,
                              const std::unordered_map<std::string, std::vector<Expr>> &paramMap,
                              const std::unordered_map<std::string, BindingInfo> &localTypes,
                              const std::unordered_map<std::string, ReturnKind> &returnKinds) const {
  std::function<bool(const Expr &)> isPointerExpr;
  isPointerExpr = [&](const Expr &candidate) -> bool {
    if (candidate.kind == Expr::Kind::Name) {
      auto it = localTypes.find(candidate.name);
      return it != localTypes.end() && it->second.typeName == "Pointer";
    }
    if (candidate.kind != Expr::Kind::Call) {
      return false;
    }
    if (isSimpleCallName(candidate, "location")) {
      return true;
    }
    char op = '\0';
    if (getBuiltinOperator(candidate, op) && candidate.args.size() == 2 && (op == '+' || op == '-')) {
      return isPointerExpr(candidate.args[0]) && !isPointerExpr(candidate.args[1]);
    }
    return false;
  };
  if (expr.kind == Expr::Kind::Literal) {
    if (!expr.isUnsigned && expr.intWidth == 32) {
      return std::to_string(static_cast<int64_t>(expr.literalValue));
    }
    if (expr.isUnsigned) {
      std::ostringstream out;
      out << "static_cast<uint64_t>(" << static_cast<uint64_t>(expr.literalValue) << ")";
      return out.str();
    }
    std::ostringstream out;
    out << "static_cast<int64_t>(" << static_cast<int64_t>(expr.literalValue) << ")";
    return out.str();
  }
  if (expr.kind == Expr::Kind::BoolLiteral) {
    return expr.boolValue ? "true" : "false";
  }
  if (expr.kind == Expr::Kind::FloatLiteral) {
    if (expr.floatWidth == 64) {
      return expr.floatValue;
    }
    return expr.floatValue + "f";
  }
  if (expr.kind == Expr::Kind::StringLiteral) {
    return stripStringLiteralSuffix(expr.stringValue);
  }
  if (expr.kind == Expr::Kind::Name) {
    return expr.name;
  }
  std::string full = resolveExprPath(expr);
  if (expr.isMethodCall && !isArrayCountCall(expr, localTypes) && !isStringCountCall(expr, localTypes)) {
    std::string methodPath;
    if (resolveMethodCallPath(expr, localTypes, returnKinds, methodPath)) {
      full = methodPath;
    }
  }
  if (isBuiltinIf(expr, nameMap) && expr.args.size() == 3) {
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
      return nameMap.count(resolved) == 0;
    };
    auto unwrapIfBranchValue = [&](const Expr &candidate) -> const Expr * {
      if (!isIfBlockEnvelope(candidate)) {
        return &candidate;
      }
      if (candidate.bodyArguments.size() != 1) {
        return nullptr;
      }
      return &candidate.bodyArguments.front();
    };
    const Expr *thenExpr = unwrapIfBranchValue(expr.args[1]);
    const Expr *elseExpr = unwrapIfBranchValue(expr.args[2]);
    if (!thenExpr || !elseExpr) {
      return "0";
    }
    std::ostringstream out;
    out << "(" << emitExpr(expr.args[0], nameMap, paramMap, localTypes, returnKinds) << " ? "
        << emitExpr(*thenExpr, nameMap, paramMap, localTypes, returnKinds) << " : "
        << emitExpr(*elseExpr, nameMap, paramMap, localTypes, returnKinds) << ")";
    return out.str();
  }
  auto it = nameMap.find(full);
  if (it == nameMap.end()) {
    if (isArrayCountCall(expr, localTypes)) {
      std::ostringstream out;
      out << "ps_array_count(" << emitExpr(expr.args.front(), nameMap, paramMap, localTypes, returnKinds) << ")";
      return out.str();
    }
    if (isStringCountCall(expr, localTypes)) {
      std::ostringstream out;
      out << "ps_string_count(" << emitExpr(expr.args.front(), nameMap, paramMap, localTypes, returnKinds) << ")";
      return out.str();
    }
    if (expr.name == "at" && expr.args.size() == 2) {
      std::ostringstream out;
      const Expr &target = expr.args[0];
      if (isStringValue(target, localTypes)) {
        out << "ps_string_at(" << emitExpr(target, nameMap, paramMap, localTypes, returnKinds) << ", "
            << emitExpr(expr.args[1], nameMap, paramMap, localTypes, returnKinds) << ")";
      } else {
        out << "ps_array_at(" << emitExpr(target, nameMap, paramMap, localTypes, returnKinds) << ", "
            << emitExpr(expr.args[1], nameMap, paramMap, localTypes, returnKinds) << ")";
      }
      return out.str();
    }
    if (expr.name == "at_unsafe" && expr.args.size() == 2) {
      std::ostringstream out;
      const Expr &target = expr.args[0];
      if (isStringValue(target, localTypes)) {
        out << "ps_string_at_unsafe(" << emitExpr(target, nameMap, paramMap, localTypes, returnKinds) << ", "
            << emitExpr(expr.args[1], nameMap, paramMap, localTypes, returnKinds) << ")";
      } else {
        out << "ps_array_at_unsafe(" << emitExpr(target, nameMap, paramMap, localTypes, returnKinds) << ", "
            << emitExpr(expr.args[1], nameMap, paramMap, localTypes, returnKinds) << ")";
      }
      return out.str();
    }
    char op = '\0';
    if (getBuiltinOperator(expr, op) && expr.args.size() == 2) {
      std::ostringstream out;
      if ((op == '+' || op == '-') && isPointerExpr(expr.args[0])) {
        out << "ps_pointer_add(" << emitExpr(expr.args[0], nameMap, paramMap, localTypes, returnKinds) << ", ";
        if (op == '-') {
          out << "-(" << emitExpr(expr.args[1], nameMap, paramMap, localTypes, returnKinds) << ")";
        } else {
          out << emitExpr(expr.args[1], nameMap, paramMap, localTypes, returnKinds);
        }
        out << ")";
        return out.str();
      }
      out << "(" << emitExpr(expr.args[0], nameMap, paramMap, localTypes, returnKinds) << " " << op << " "
          << emitExpr(expr.args[1], nameMap, paramMap, localTypes, returnKinds) << ")";
      return out.str();
    }
    if (isBuiltinNegate(expr) && expr.args.size() == 1) {
      std::ostringstream out;
      out << "(-" << emitExpr(expr.args[0], nameMap, paramMap, localTypes, returnKinds) << ")";
      return out.str();
    }
    const char *cmp = nullptr;
    if (getBuiltinComparison(expr, cmp)) {
      std::ostringstream out;
      if (expr.args.size() == 1) {
        out << "(" << cmp << emitExpr(expr.args[0], nameMap, paramMap, localTypes, returnKinds) << ")";
        return out.str();
      }
      if (expr.args.size() == 2) {
        out << "(" << emitExpr(expr.args[0], nameMap, paramMap, localTypes, returnKinds) << " " << cmp << " "
            << emitExpr(expr.args[1], nameMap, paramMap, localTypes, returnKinds) << ")";
        return out.str();
      }
    }
    if (isBuiltinClamp(expr) && expr.args.size() == 3) {
      std::ostringstream out;
      out << "ps_builtin_clamp(" << emitExpr(expr.args[0], nameMap, paramMap, localTypes, returnKinds) << ", "
          << emitExpr(expr.args[1], nameMap, paramMap, localTypes, returnKinds) << ", "
          << emitExpr(expr.args[2], nameMap, paramMap, localTypes, returnKinds) << ")";
      return out.str();
    }
    std::string convertName;
    if (getBuiltinConvertName(expr, convertName) && expr.templateArgs.size() == 1 && expr.args.size() == 1) {
      std::string targetType = bindingTypeToCpp(expr.templateArgs[0]);
      std::ostringstream out;
      out << "static_cast<" << targetType << ">("
          << emitExpr(expr.args[0], nameMap, paramMap, localTypes, returnKinds) << ")";
      return out.str();
    }
    char pointerOp = '\0';
    if (getBuiltinPointerOperator(expr, pointerOp) && expr.args.size() == 1) {
      std::ostringstream out;
      out << "(" << pointerOp << emitExpr(expr.args[0], nameMap, paramMap, localTypes, returnKinds) << ")";
      return out.str();
    }
    std::string collection;
    if (getBuiltinCollectionName(expr, collection)) {
      if (collection == "array" && expr.templateArgs.size() == 1) {
        std::ostringstream out;
        const std::string elemType = bindingTypeToCpp(expr.templateArgs[0]);
        out << "std::vector<" << elemType << ">{";
        for (size_t i = 0; i < expr.args.size(); ++i) {
          if (i > 0) {
            out << ", ";
          }
          out << emitExpr(expr.args[i], nameMap, paramMap, localTypes, returnKinds);
        }
        out << "}";
        return out.str();
      }
      if (collection == "map" && expr.templateArgs.size() == 2) {
        std::ostringstream out;
        const std::string keyType = bindingTypeToCpp(expr.templateArgs[0]);
        const std::string valueType = bindingTypeToCpp(expr.templateArgs[1]);
        out << "std::unordered_map<" << keyType << ", " << valueType << ">{";
        for (size_t i = 0; i + 1 < expr.args.size(); i += 2) {
          if (i > 0) {
            out << ", ";
          }
          out << "{" << emitExpr(expr.args[i], nameMap, paramMap, localTypes, returnKinds) << ", "
              << emitExpr(expr.args[i + 1], nameMap, paramMap, localTypes, returnKinds) << "}";
        }
        out << "}";
        return out.str();
      }
    }
    if (isBuiltinAssign(expr, nameMap) && expr.args.size() == 2) {
      std::ostringstream out;
      out << "(" << emitExpr(expr.args[0], nameMap, paramMap, localTypes, returnKinds) << " = "
          << emitExpr(expr.args[1], nameMap, paramMap, localTypes, returnKinds) << ")";
      return out.str();
    }
    return "0";
  }
  std::ostringstream out;
  out << it->second << "(";
  std::vector<const Expr *> orderedArgs;
  auto paramIt = paramMap.find(full);
  if (paramIt != paramMap.end()) {
    orderedArgs = orderCallArguments(expr, paramIt->second);
  } else {
    for (const auto &arg : expr.args) {
      orderedArgs.push_back(&arg);
    }
  }
  for (size_t i = 0; i < orderedArgs.size(); ++i) {
    if (i > 0) {
      out << ", ";
    }
    out << emitExpr(*orderedArgs[i], nameMap, paramMap, localTypes, returnKinds);
  }
  out << ")";
  return out.str();
}

std::string Emitter::emitCpp(const Program &program, const std::string &entryPath) const {
  std::unordered_map<std::string, std::string> nameMap;
  std::unordered_map<std::string, std::vector<Expr>> paramMap;
  std::unordered_map<std::string, const Definition *> defMap;
  std::unordered_map<std::string, ReturnKind> returnKinds;
  for (const auto &def : program.definitions) {
    nameMap[def.fullPath] = toCppName(def.fullPath);
    paramMap[def.fullPath] = def.parameters;
    defMap[def.fullPath] = &def;
    returnKinds[def.fullPath] = getReturnKind(def);
  }

  auto isParam = [](const std::vector<Expr> &params, const std::string &name) -> bool {
    for (const auto &param : params) {
      if (param.name == name) {
        return true;
      }
    }
    return false;
  };

  std::unordered_set<std::string> inferenceStack;
  std::function<ReturnKind(const Definition &)> inferDefinitionReturnKind;
  std::function<ReturnKind(const Expr &,
                           const std::vector<Expr> &,
                           const std::unordered_map<std::string, ReturnKind> &)>
      inferExprReturnKind;

  inferExprReturnKind = [&](const Expr &expr,
                            const std::vector<Expr> &params,
                            const std::unordered_map<std::string, ReturnKind> &locals) -> ReturnKind {
    auto combineNumeric = [&](ReturnKind left, ReturnKind right) -> ReturnKind {
      if (left == ReturnKind::Unknown || right == ReturnKind::Unknown) {
        return ReturnKind::Unknown;
      }
      if (left == ReturnKind::Bool || right == ReturnKind::Bool || left == ReturnKind::Void || right == ReturnKind::Void) {
        return ReturnKind::Unknown;
      }
      if (left == ReturnKind::Float64 || right == ReturnKind::Float64) {
        return ReturnKind::Float64;
      }
      if (left == ReturnKind::Float32 || right == ReturnKind::Float32) {
        return ReturnKind::Float32;
      }
      if (left == ReturnKind::UInt64 || right == ReturnKind::UInt64) {
        return (left == ReturnKind::UInt64 && right == ReturnKind::UInt64) ? ReturnKind::UInt64 : ReturnKind::Unknown;
      }
      if (left == ReturnKind::Int64 || right == ReturnKind::Int64) {
        if (left == ReturnKind::Int64 || left == ReturnKind::Int) {
          if (right == ReturnKind::Int64 || right == ReturnKind::Int) {
            return ReturnKind::Int64;
          }
        }
        return ReturnKind::Unknown;
      }
      if (left == ReturnKind::Int && right == ReturnKind::Int) {
        return ReturnKind::Int;
      }
      return ReturnKind::Unknown;
    };

    if (expr.kind == Expr::Kind::Literal) {
      if (expr.isUnsigned) {
        return ReturnKind::UInt64;
      }
      if (expr.intWidth == 64) {
        return ReturnKind::Int64;
      }
      return ReturnKind::Int;
    }
    if (expr.kind == Expr::Kind::BoolLiteral) {
      return ReturnKind::Bool;
    }
    if (expr.kind == Expr::Kind::FloatLiteral) {
      return expr.floatWidth == 64 ? ReturnKind::Float64 : ReturnKind::Float32;
    }
    if (expr.kind == Expr::Kind::StringLiteral) {
      return ReturnKind::Unknown;
    }
    if (expr.kind == Expr::Kind::Name) {
      if (isParam(params, expr.name)) {
        return ReturnKind::Unknown;
      }
      auto it = locals.find(expr.name);
      if (it == locals.end()) {
        return ReturnKind::Unknown;
      }
      return it->second;
    }
    if (expr.kind == Expr::Kind::Call) {
      std::string full = resolveExprPath(expr);
      auto defIt = defMap.find(full);
      if (defIt != defMap.end()) {
        ReturnKind calleeKind = inferDefinitionReturnKind(*defIt->second);
        if (calleeKind == ReturnKind::Void || calleeKind == ReturnKind::Unknown) {
          return ReturnKind::Unknown;
        }
        return calleeKind;
      }
      if (isBuiltinIf(expr, nameMap) && expr.args.size() == 3) {
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
        auto unwrapIfBranchValue = [&](const Expr &candidate) -> const Expr * {
          if (!isIfBlockEnvelope(candidate)) {
            return &candidate;
          }
          if (candidate.bodyArguments.size() != 1) {
            return nullptr;
          }
          return &candidate.bodyArguments.front();
        };
        const Expr *thenExpr = unwrapIfBranchValue(expr.args[1]);
        const Expr *elseExpr = unwrapIfBranchValue(expr.args[2]);
        if (!thenExpr || !elseExpr) {
          return ReturnKind::Unknown;
        }
        ReturnKind thenKind = inferExprReturnKind(*thenExpr, params, locals);
        ReturnKind elseKind = inferExprReturnKind(*elseExpr, params, locals);
        if (thenKind == elseKind) {
          return thenKind;
        }
        return combineNumeric(thenKind, elseKind);
      }
      const char *cmp = nullptr;
      if (getBuiltinComparison(expr, cmp)) {
        return ReturnKind::Bool;
      }
      char op = '\0';
      if (getBuiltinOperator(expr, op)) {
        if (expr.args.size() != 2) {
          return ReturnKind::Unknown;
        }
        ReturnKind left = inferExprReturnKind(expr.args[0], params, locals);
        ReturnKind right = inferExprReturnKind(expr.args[1], params, locals);
        return combineNumeric(left, right);
      }
      if (isBuiltinNegate(expr)) {
        if (expr.args.size() != 1) {
          return ReturnKind::Unknown;
        }
        ReturnKind argKind = inferExprReturnKind(expr.args.front(), params, locals);
        if (argKind == ReturnKind::Bool || argKind == ReturnKind::Void) {
          return ReturnKind::Unknown;
        }
        return argKind;
      }
      if (isBuiltinClamp(expr)) {
        if (expr.args.size() != 3) {
          return ReturnKind::Unknown;
        }
        ReturnKind result = inferExprReturnKind(expr.args[0], params, locals);
        result = combineNumeric(result, inferExprReturnKind(expr.args[1], params, locals));
        result = combineNumeric(result, inferExprReturnKind(expr.args[2], params, locals));
        return result;
      }
      std::string convertName;
      if (getBuiltinConvertName(expr, convertName) && expr.templateArgs.size() == 1) {
        return returnKindForTypeName(expr.templateArgs.front());
      }
      return ReturnKind::Unknown;
    }
    return ReturnKind::Unknown;
  };

  inferDefinitionReturnKind = [&](const Definition &def) -> ReturnKind {
    ReturnKind &kind = returnKinds[def.fullPath];
    if (kind != ReturnKind::Unknown) {
      return kind;
    }
    if (!inferenceStack.insert(def.fullPath).second) {
      return ReturnKind::Unknown;
    }
    ReturnKind inferred = ReturnKind::Unknown;
    bool sawReturn = false;
    std::unordered_map<std::string, ReturnKind> locals;
    std::function<void(const Expr &, std::unordered_map<std::string, ReturnKind> &)> visitStmt;
    visitStmt = [&](const Expr &stmt, std::unordered_map<std::string, ReturnKind> &activeLocals) {
      if (stmt.isBinding) {
        BindingInfo info = getBindingInfo(stmt);
        ReturnKind bindingKind = returnKindForTypeName(info.typeName);
        if (!hasExplicitBindingTypeTransform(stmt) && stmt.args.size() == 1) {
          ReturnKind initKind = inferExprReturnKind(stmt.args.front(), def.parameters, activeLocals);
          if (initKind != ReturnKind::Unknown && initKind != ReturnKind::Void) {
            bindingKind = initKind;
          }
        }
        activeLocals.emplace(stmt.name, bindingKind);
        return;
      }
      if (isReturnCall(stmt)) {
        sawReturn = true;
        ReturnKind exprKind = ReturnKind::Void;
        if (!stmt.args.empty()) {
          exprKind = inferExprReturnKind(stmt.args.front(), def.parameters, activeLocals);
        }
        if (exprKind == ReturnKind::Unknown) {
          inferred = ReturnKind::Unknown;
          return;
        }
        if (inferred == ReturnKind::Unknown) {
          inferred = exprKind;
          return;
        }
        if (inferred != exprKind) {
          inferred = ReturnKind::Unknown;
          return;
        }
        return;
      }
      if (isBuiltinIf(stmt, nameMap) && stmt.args.size() == 3) {
        const Expr &thenBlock = stmt.args[1];
        const Expr &elseBlock = stmt.args[2];
        auto walkBlock = [&](const Expr &block) {
          std::unordered_map<std::string, ReturnKind> blockLocals = activeLocals;
          for (const auto &bodyStmt : block.bodyArguments) {
            visitStmt(bodyStmt, blockLocals);
          }
        };
        walkBlock(thenBlock);
        walkBlock(elseBlock);
        return;
      }
    };
    for (const auto &stmt : def.statements) {
      visitStmt(stmt, locals);
    }
    if (!sawReturn) {
      kind = ReturnKind::Void;
    } else if (inferred == ReturnKind::Unknown) {
      kind = ReturnKind::Int;
    } else {
      kind = inferred;
    }
    inferenceStack.erase(def.fullPath);
    return kind;
  };

  for (const auto &def : program.definitions) {
    if (returnKinds[def.fullPath] == ReturnKind::Unknown) {
      inferDefinitionReturnKind(def);
    }
  }

  std::ostringstream out;
  out << "// Generated by primec (minimal)\n";
  out << "#include <cstdint>\n";
  out << "#include <cstdio>\n";
  out << "#include <cstdlib>\n";
  out << "#include <cstring>\n";
  out << "#include <string>\n";
  out << "#include <string_view>\n";
  out << "#include <type_traits>\n";
  out << "#include <unordered_map>\n";
  out << "#include <vector>\n";
  out << "template <typename T, typename Offset>\n";
  out << "static inline T *ps_pointer_add(T *ptr, Offset offset) {\n";
  out << "  if constexpr (std::is_signed_v<Offset>) {\n";
  out << "    return reinterpret_cast<T *>(reinterpret_cast<std::intptr_t>(ptr) + "
         "static_cast<std::intptr_t>(offset));\n";
  out << "  }\n";
  out << "  return reinterpret_cast<T *>(reinterpret_cast<std::uintptr_t>(ptr) + "
         "static_cast<std::uintptr_t>(offset));\n";
  out << "}\n";
  out << "template <typename T, typename Offset>\n";
  out << "static inline T *ps_pointer_add(T &ref, Offset offset) {\n";
  out << "  return ps_pointer_add(&ref, offset);\n";
  out << "}\n";
  out << "template <typename T, typename U, typename V>\n";
  out << "static inline std::common_type_t<T, U, V> ps_builtin_clamp(T value, U minValue, V maxValue) {\n";
  out << "  using R = std::common_type_t<T, U, V>;\n";
  out << "  R v = static_cast<R>(value);\n";
  out << "  R minV = static_cast<R>(minValue);\n";
  out << "  R maxV = static_cast<R>(maxValue);\n";
  out << "  if (v < minV) {\n";
  out << "    return minV;\n";
  out << "  }\n";
  out << "  if (v > maxV) {\n";
  out << "    return maxV;\n";
  out << "  }\n";
  out << "  return v;\n";
  out << "}\n";
  out << "template <typename T>\n";
  out << "static inline int ps_array_count(const std::vector<T> &value) {\n";
  out << "  return static_cast<int>(value.size());\n";
  out << "}\n";
  out << "template <typename T, typename Index>\n";
  out << "static inline const T &ps_array_at(const std::vector<T> &value, Index index) {\n";
  out << "  int64_t i = static_cast<int64_t>(index);\n";
  out << "  if (i < 0 || static_cast<size_t>(i) >= value.size()) {\n";
  out << "    std::fprintf(stderr, \"array index out of bounds\\n\");\n";
  out << "    std::exit(3);\n";
  out << "  }\n";
  out << "  return value[static_cast<size_t>(i)];\n";
  out << "}\n";
  out << "template <typename T, typename Index>\n";
  out << "static inline const T &ps_array_at_unsafe(const std::vector<T> &value, Index index) {\n";
  out << "  return value[static_cast<size_t>(index)];\n";
  out << "}\n";
  out << "static inline int ps_string_count(std::string_view value) {\n";
  out << "  return static_cast<int>(value.size());\n";
  out << "}\n";
  out << "template <typename Index>\n";
  out << "static inline int ps_string_at(std::string_view value, Index index) {\n";
  out << "  int64_t i = static_cast<int64_t>(index);\n";
  out << "  if (i < 0 || static_cast<size_t>(i) >= value.size()) {\n";
  out << "    std::fprintf(stderr, \"string index out of bounds\\n\");\n";
  out << "    std::exit(3);\n";
  out << "  }\n";
  out << "  return static_cast<unsigned char>(value[static_cast<size_t>(i)]);\n";
  out << "}\n";
  out << "template <typename Index>\n";
  out << "static inline int ps_string_at_unsafe(std::string_view value, Index index) {\n";
  out << "  return static_cast<unsigned char>(value[static_cast<size_t>(index)]);\n";
  out << "}\n";
  out << "static inline void ps_write(FILE *stream, const char *data, size_t len, bool newline) {\n";
  out << "  if (len > 0) {\n";
  out << "    std::fwrite(data, 1, len, stream);\n";
  out << "  }\n";
  out << "  if (newline) {\n";
  out << "    std::fputc('\\n', stream);\n";
  out << "  }\n";
  out << "}\n";
  out << "static inline void ps_print_value(FILE *stream, const char *text, bool newline) {\n";
  out << "  if (!text) {\n";
  out << "    if (newline) {\n";
  out << "      std::fputc('\\n', stream);\n";
  out << "    }\n";
  out << "    return;\n";
  out << "  }\n";
  out << "  ps_write(stream, text, std::strlen(text), newline);\n";
  out << "}\n";
  out << "static inline void ps_print_value(FILE *stream, std::string_view text, bool newline) {\n";
  out << "  ps_write(stream, text.data(), text.size(), newline);\n";
  out << "}\n";
  out << "template <typename T>\n";
  out << "static inline void ps_print_value(FILE *stream, T value, bool newline) {\n";
  out << "  std::string text = std::to_string(value);\n";
  out << "  ps_write(stream, text.c_str(), text.size(), newline);\n";
  out << "}\n";
  for (const auto &def : program.definitions) {
    ReturnKind returnKind = returnKinds[def.fullPath];
    std::string returnType = "int";
    if (returnKind == ReturnKind::Void) {
      returnType = "void";
    } else if (returnKind == ReturnKind::Int64) {
      returnType = "int64_t";
    } else if (returnKind == ReturnKind::UInt64) {
      returnType = "uint64_t";
    } else if (returnKind == ReturnKind::Float32) {
      returnType = "float";
    } else if (returnKind == ReturnKind::Float64) {
      returnType = "double";
    } else if (returnKind == ReturnKind::Bool) {
      returnType = "bool";
    }
    out << "static " << returnType << " " << nameMap[def.fullPath] << "(";
    for (size_t i = 0; i < def.parameters.size(); ++i) {
      if (i > 0) {
        out << ", ";
      }
      BindingInfo paramInfo = getBindingInfo(def.parameters[i]);
      std::string paramType = bindingTypeToCpp(paramInfo);
      bool needsConst = !paramInfo.isMutable;
      if (needsConst && paramType.rfind("const ", 0) == 0) {
        needsConst = false;
      }
      out << (needsConst ? "const " : "") << paramType << " " << def.parameters[i].name;
    }
    out << ") {\n";
    std::function<void(const Expr &, int, std::unordered_map<std::string, BindingInfo> &)> emitStatement;
    int repeatCounter = 0;
    emitStatement = [&](const Expr &stmt, int indent, std::unordered_map<std::string, BindingInfo> &localTypes) {
      std::string pad(static_cast<size_t>(indent) * 2, ' ');
      if (isReturnCall(stmt)) {
        out << pad << "return";
        if (!stmt.args.empty()) {
          out << " " << emitExpr(stmt.args.front(), nameMap, paramMap, localTypes, returnKinds);
        }
        out << ";\n";
        return;
      }
      if (!stmt.isBinding && stmt.kind == Expr::Kind::Call) {
        PrintBuiltin printBuiltin;
        if (getPrintBuiltin(stmt, printBuiltin)) {
          const char *stream = (printBuiltin.target == PrintTarget::Err) ? "stderr" : "stdout";
          std::string argText = "\"\"";
          if (!stmt.args.empty()) {
            argText = emitExpr(stmt.args.front(), nameMap, paramMap, localTypes, returnKinds);
          }
          out << pad << "ps_print_value(" << stream << ", " << argText << ", "
              << (printBuiltin.newline ? "true" : "false") << ");\n";
          return;
        }
      }
      if (stmt.isBinding) {
        BindingInfo binding = getBindingInfo(stmt);
        if (!hasExplicitBindingTypeTransform(stmt) && stmt.args.size() == 1) {
          std::unordered_map<std::string, ReturnKind> locals;
          locals.reserve(localTypes.size());
          for (const auto &entry : localTypes) {
            locals.emplace(entry.first, returnKindForTypeName(entry.second.typeName));
          }
          ReturnKind initKind = inferExprReturnKind(stmt.args.front(), def.parameters, locals);
          std::string inferred = typeNameForReturnKind(initKind);
          if (!inferred.empty()) {
            binding.typeName = inferred;
            binding.typeTemplateArg.clear();
          }
        }
        std::string type = bindingTypeToCpp(binding);
        bool isReference = binding.typeName == "Reference";
        localTypes[stmt.name] = binding;
        bool needsConst = !binding.isMutable;
        if (needsConst && type.rfind("const ", 0) == 0) {
          needsConst = false;
        }
        out << pad << (needsConst ? "const " : "") << type << " " << stmt.name;
        if (!stmt.args.empty()) {
          if (isReference) {
            out << " = *(" << emitExpr(stmt.args.front(), nameMap, paramMap, localTypes, returnKinds) << ")";
          } else {
            out << " = " << emitExpr(stmt.args.front(), nameMap, paramMap, localTypes, returnKinds);
          }
        }
        out << ";\n";
        return;
      }
      if (isBuiltinIf(stmt, nameMap) && stmt.args.size() == 3) {
        const Expr &cond = stmt.args[0];
        const Expr &thenArg = stmt.args[1];
        const Expr &elseArg = stmt.args[2];
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
          return nameMap.count(resolved) == 0;
        };
        out << pad << "if (" << emitExpr(cond, nameMap, paramMap, localTypes, returnKinds) << ") {\n";
        {
          auto blockTypes = localTypes;
          if (isIfBlockEnvelope(thenArg)) {
            for (const auto &bodyStmt : thenArg.bodyArguments) {
              emitStatement(bodyStmt, indent + 1, blockTypes);
            }
          } else {
            emitStatement(thenArg, indent + 1, blockTypes);
          }
        }
        out << pad << "} else {\n";
        {
          auto blockTypes = localTypes;
          if (isIfBlockEnvelope(elseArg)) {
            for (const auto &bodyStmt : elseArg.bodyArguments) {
              emitStatement(bodyStmt, indent + 1, blockTypes);
            }
          } else {
            emitStatement(elseArg, indent + 1, blockTypes);
          }
        }
        out << pad << "}\n";
        return;
      }
      if (stmt.kind == Expr::Kind::Call && isRepeatCall(stmt)) {
        const int repeatIndex = repeatCounter++;
        const std::string endVar = "ps_repeat_end_" + std::to_string(repeatIndex);
        const std::string indexVar = "ps_repeat_i_" + std::to_string(repeatIndex);
        out << pad << "{\n";
        std::string innerPad(static_cast<size_t>(indent + 1) * 2, ' ');
        std::string countExpr = "\"\"";
        if (!stmt.args.empty()) {
          countExpr = emitExpr(stmt.args.front(), nameMap, paramMap, localTypes, returnKinds);
        }
        out << innerPad << "auto " << endVar << " = " << countExpr << ";\n";
        out << innerPad << "for (decltype(" << endVar << ") " << indexVar << " = 0; " << indexVar << " < " << endVar
            << "; ++" << indexVar << ") {\n";
        auto blockTypes = localTypes;
        for (const auto &bodyStmt : stmt.bodyArguments) {
          emitStatement(bodyStmt, indent + 2, blockTypes);
        }
        out << innerPad << "}\n";
        out << pad << "}\n";
        return;
      }
      if (stmt.kind == Expr::Kind::Call && (stmt.hasBodyArguments || !stmt.bodyArguments.empty())) {
        std::string full = resolveExprPath(stmt);
        if (stmt.isMethodCall && !isArrayCountCall(stmt, localTypes)) {
          std::string methodPath;
          if (resolveMethodCallPath(stmt, localTypes, returnKinds, methodPath)) {
            full = methodPath;
          }
        }
        auto it = nameMap.find(full);
        if (it == nameMap.end()) {
          out << pad << emitExpr(stmt, nameMap, paramMap, localTypes, returnKinds) << ";\n";
          return;
        }
        out << pad << it->second << "(";
        std::vector<const Expr *> orderedArgs;
        auto paramIt = paramMap.find(full);
        if (paramIt != paramMap.end()) {
          orderedArgs = orderCallArguments(stmt, paramIt->second);
        } else {
          for (const auto &arg : stmt.args) {
            orderedArgs.push_back(&arg);
          }
        }
        for (size_t i = 0; i < orderedArgs.size(); ++i) {
          if (i > 0) {
            out << ", ";
          }
          out << emitExpr(*orderedArgs[i], nameMap, paramMap, localTypes, returnKinds);
        }
        if (!orderedArgs.empty()) {
          out << ", ";
        }
        out << "[&]() {\n";
        for (const auto &bodyStmt : stmt.bodyArguments) {
          emitStatement(bodyStmt, indent + 1, localTypes);
        }
        out << pad << "}";
        out << ");\n";
        return;
      }
      if (stmt.kind == Expr::Kind::Call && isBuiltinAssign(stmt, nameMap) && stmt.args.size() == 2 &&
          stmt.args.front().kind == Expr::Kind::Name) {
        out << pad << stmt.args.front().name << " = "
            << emitExpr(stmt.args[1], nameMap, paramMap, localTypes, returnKinds) << ";\n";
        return;
      }
      if (stmt.kind == Expr::Kind::Call && isPathSpaceBuiltinName(stmt) && nameMap.find(resolveExprPath(stmt)) == nameMap.end()) {
        for (const auto &arg : stmt.args) {
          out << pad << "(void)(" << emitExpr(arg, nameMap, paramMap, localTypes, returnKinds) << ");\n";
        }
        return;
      }
      out << pad << emitExpr(stmt, nameMap, paramMap, localTypes, returnKinds) << ";\n";
    };
    std::unordered_map<std::string, BindingInfo> localTypes;
    for (const auto &param : def.parameters) {
      localTypes[param.name] = getBindingInfo(param);
    }
    for (const auto &stmt : def.statements) {
      emitStatement(stmt, 1, localTypes);
    }
    if (returnKind == ReturnKind::Void) {
      out << "  return;\n";
    }
    out << "}\n";
  }
  ReturnKind entryReturn = ReturnKind::Int;
  const Definition *entryDef = nullptr;
  for (const auto &def : program.definitions) {
    if (def.fullPath == entryPath) {
      entryReturn = returnKinds[def.fullPath];
      entryDef = &def;
      break;
    }
  }
  bool entryHasArgs = false;
  if (entryDef && entryDef->parameters.size() == 1) {
    BindingInfo paramInfo = getBindingInfo(entryDef->parameters.front());
    if (paramInfo.typeName == "array" && paramInfo.typeTemplateArg == "string") {
      entryHasArgs = true;
    }
  }
  if (entryHasArgs) {
    out << "int main(int argc, char **argv) {\n";
    out << "  std::vector<std::string_view> ps_args;\n";
    out << "  ps_args.reserve(static_cast<size_t>(argc));\n";
    out << "  for (int i = 0; i < argc; ++i) {\n";
    out << "    ps_args.emplace_back(argv[i]);\n";
    out << "  }\n";
    if (entryReturn == ReturnKind::Void) {
      out << "  " << nameMap.at(entryPath) << "(ps_args);\n";
      out << "  return 0;\n";
    } else if (entryReturn == ReturnKind::Int) {
      out << "  return " << nameMap.at(entryPath) << "(ps_args);\n";
    } else {
      out << "  return static_cast<int>(" << nameMap.at(entryPath) << "(ps_args));\n";
    }
    out << "}\n";
  } else {
    if (entryReturn == ReturnKind::Void) {
      out << "int main() { " << nameMap.at(entryPath) << "(); return 0; }\n";
    } else if (entryReturn == ReturnKind::Int) {
      out << "int main() { return " << nameMap.at(entryPath) << "(); }\n";
    } else if (entryReturn == ReturnKind::Int64 || entryReturn == ReturnKind::UInt64) {
      out << "int main() { return static_cast<int>(" << nameMap.at(entryPath) << "()); }\n";
    } else {
      out << "int main() { return static_cast<int>(" << nameMap.at(entryPath) << "()); }\n";
    }
  }
  return out.str();
}

} // namespace primec
