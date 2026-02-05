#include "primec/Semantics.h"
#include "primec/StringLiteral.h"

#include <cctype>
#include <functional>
#include <limits>
#include <unordered_map>
#include <unordered_set>

namespace primec {
namespace {
enum class ReturnKind { Unknown, Int, Int64, UInt64, Float32, Float64, Bool, Void };

struct BindingInfo {
  std::string typeName;
  std::string typeTemplateArg;
  bool isMutable = false;
};

struct ParameterInfo {
  std::string name;
  BindingInfo binding;
  const Expr *defaultExpr = nullptr;
};

bool validateAlignTransform(const Transform &transform, const std::string &context, std::string &error);
std::string resolveTypePath(const std::string &name, const std::string &namespacePrefix);

bool isBindingQualifierName(const std::string &name) {
  return name == "public" || name == "private" || name == "package" || name == "static";
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

bool splitTemplateTypeName(const std::string &text, std::string &base, std::string &arg) {
  size_t lt = text.find('<');
  if (lt == std::string::npos) {
    base = text;
    arg.clear();
    return false;
  }
  size_t gt = text.rfind('>');
  if (gt == std::string::npos || gt <= lt || gt != text.size() - 1) {
    base = text;
    arg.clear();
    return false;
  }
  base = text.substr(0, lt);
  arg = text.substr(lt + 1, gt - lt - 1);
  return true;
}

bool isBuiltinTemplateTypeName(const std::string &name) {
  return name == "Pointer" || name == "Reference";
}

bool restrictMatchesBinding(const std::string &restrictType,
                            const std::string &typeName,
                            const std::string &typeTemplateArg,
                            bool typeHasTemplate,
                            const std::string &namespacePrefix) {
  std::string restrictBase;
  std::string restrictArg;
  bool restrictHasTemplate = splitTemplateTypeName(restrictType, restrictBase, restrictArg);
  if (restrictHasTemplate != typeHasTemplate) {
    return false;
  }
  if (typeHasTemplate) {
    std::string bindingBase = typeName;
    std::string bindingArg = typeTemplateArg;
    std::string resolvedBindingBase = bindingBase;
    std::string resolvedRestrictBase = restrictBase;
    if (isPrimitiveBindingTypeName(bindingBase) || isPrimitiveBindingTypeName(restrictBase)) {
      resolvedBindingBase = normalizeBindingTypeName(bindingBase);
      resolvedRestrictBase = normalizeBindingTypeName(restrictBase);
    } else {
      if (!bindingBase.empty() && bindingBase[0] != '/' && !isBuiltinTemplateTypeName(bindingBase)) {
        resolvedBindingBase = resolveTypePath(bindingBase, namespacePrefix);
      }
      if (!restrictBase.empty() && restrictBase[0] != '/' && !isBuiltinTemplateTypeName(restrictBase)) {
        resolvedRestrictBase = resolveTypePath(restrictBase, namespacePrefix);
      }
    }
    if (resolvedBindingBase != resolvedRestrictBase) {
      return false;
    }
    if (isBuiltinTemplateTypeName(bindingBase)) {
      return normalizeBindingTypeName(bindingArg) == normalizeBindingTypeName(restrictArg);
    }
    if (isPrimitiveBindingTypeName(bindingArg) || isPrimitiveBindingTypeName(restrictArg)) {
      return normalizeBindingTypeName(bindingArg) == normalizeBindingTypeName(restrictArg);
    }
    return bindingArg == restrictArg;
  }
  if (isPrimitiveBindingTypeName(typeName) || isPrimitiveBindingTypeName(restrictType)) {
    return normalizeBindingTypeName(typeName) == normalizeBindingTypeName(restrictType);
  }
  return resolveTypePath(typeName, namespacePrefix) == resolveTypePath(restrictType, namespacePrefix);
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

ReturnKind getReturnKind(const Definition &def, std::string &error) {
  ReturnKind kind = ReturnKind::Unknown;
  bool sawReturn = false;
  for (const auto &transform : def.transforms) {
    if (transform.name != "return") {
      continue;
    }
    if (!transform.templateArg) {
      error = "return transform requires a type on " + def.fullPath;
      return ReturnKind::Unknown;
    }
    ReturnKind nextKind = returnKindForTypeName(*transform.templateArg);
    if (nextKind == ReturnKind::Unknown) {
      error = "unsupported return type on " + def.fullPath;
      return ReturnKind::Unknown;
    }
    if (sawReturn) {
      if (nextKind == kind) {
        error = "duplicate return transform on " + def.fullPath;
        return ReturnKind::Unknown;
      }
      error = "conflicting return types on " + def.fullPath;
      return ReturnKind::Unknown;
    }
    sawReturn = true;
    kind = nextKind;
  }
  return kind;
}

bool getBuiltinOperatorName(const Expr &expr, std::string &out) {
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
  if (name == "plus" || name == "minus" || name == "multiply" || name == "divide" || name == "negate") {
    out = name;
    return true;
  }
  return false;
}

bool getBuiltinComparisonName(const Expr &expr, std::string &out) {
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
  if (name == "greater_than" || name == "less_than" || name == "equal" || name == "not_equal" ||
      name == "greater_equal" || name == "less_equal" || name == "and" || name == "or" || name == "not") {
    out = name;
    return true;
  }
  return false;
}

bool getBuiltinClampName(const Expr &expr, std::string &out) {
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
  if (name == "clamp") {
    out = name;
    return true;
  }
  return false;
}

bool getBuiltinPointerName(const Expr &expr, std::string &out) {
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
  if (name == "dereference" || name == "location") {
    out = name;
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

bool getBuiltinArrayAccessName(const Expr &expr, std::string &out) {
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
  if (name == "at" || name == "at_unsafe") {
    out = name;
    return true;
  }
  return false;
}

bool isAssignCall(const Expr &expr) {
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
  return name == "assign";
}

const BindingInfo *findBinding(const std::vector<ParameterInfo> &params,
                               const std::unordered_map<std::string, BindingInfo> &locals,
                               const std::string &name) {
  auto it = locals.find(name);
  if (it != locals.end()) {
    return &it->second;
  }
  for (const auto &param : params) {
    if (param.name == name) {
      return &param.binding;
    }
  }
  return nullptr;
}

bool isPointerExpr(const Expr &expr,
                   const std::vector<ParameterInfo> &params,
                   const std::unordered_map<std::string, BindingInfo> &locals) {
  if (expr.kind == Expr::Kind::Name) {
    const BindingInfo *binding = findBinding(params, locals, expr.name);
    return binding != nullptr && binding->typeName == "Pointer";
  }
  if (expr.kind != Expr::Kind::Call) {
    return false;
  }
  std::string builtinName;
  if (getBuiltinPointerName(expr, builtinName) && builtinName == "location" && expr.args.size() == 1) {
    return true;
  }
  if (getBuiltinOperatorName(expr, builtinName) && (builtinName == "plus" || builtinName == "minus") &&
      expr.args.size() == 2) {
    return isPointerExpr(expr.args[0], params, locals) && !isPointerExpr(expr.args[1], params, locals);
  }
  return false;
}

bool isPointerLikeExpr(const Expr &expr,
                       const std::vector<ParameterInfo> &params,
                       const std::unordered_map<std::string, BindingInfo> &locals) {
  if (expr.kind == Expr::Kind::Name) {
    const BindingInfo *binding = findBinding(params, locals, expr.name);
    return binding != nullptr && (binding->typeName == "Pointer" || binding->typeName == "Reference");
  }
  if (expr.kind != Expr::Kind::Call) {
    return false;
  }
  std::string builtinName;
  if (getBuiltinPointerName(expr, builtinName) && builtinName == "location" && expr.args.size() == 1) {
    return true;
  }
  if (getBuiltinOperatorName(expr, builtinName) && (builtinName == "plus" || builtinName == "minus") &&
      expr.args.size() == 2) {
    return isPointerLikeExpr(expr.args[0], params, locals) && !isPointerLikeExpr(expr.args[1], params, locals);
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

bool isIfCall(const Expr &expr) {
  return isSimpleCallName(expr, "if");
}

bool isThenCall(const Expr &expr) {
  return isSimpleCallName(expr, "then");
}

bool isElseCall(const Expr &expr) {
  return isSimpleCallName(expr, "else");
}

bool isReturnCall(const Expr &expr) {
  return isSimpleCallName(expr, "return");
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

std::string resolveTypePath(const std::string &name, const std::string &namespacePrefix) {
  if (!name.empty() && name[0] == '/') {
    return name;
  }
  if (!namespacePrefix.empty()) {
    return namespacePrefix + "/" + name;
  }
  return "/" + name;
}

bool parseBindingInfo(const Expr &expr,
                      const std::string &namespacePrefix,
                      const std::unordered_set<std::string> &structTypes,
                      BindingInfo &info,
                      std::string &error) {
  std::string typeName;
  bool typeHasTemplate = false;
  std::optional<std::string> restrictType;
  for (const auto &transform : expr.transforms) {
    if (transform.name == "effects" || transform.name == "capabilities") {
      error = "binding does not accept " + transform.name + " transform";
      return false;
    }
    if (transform.name == "mut") {
      if (transform.templateArg) {
        error = "binding transforms do not take template arguments";
        return false;
      }
      if (!transform.arguments.empty()) {
        error = "binding transforms do not take arguments";
        return false;
      }
      info.isMutable = true;
      continue;
    }
    if (transform.name == "copy") {
      if (transform.templateArg) {
        error = "binding transforms do not take template arguments";
        return false;
      }
      if (!transform.arguments.empty()) {
        error = "binding transforms do not take arguments";
        return false;
      }
      continue;
    }
    if (transform.name == "restrict") {
      if (!transform.templateArg) {
        error = "restrict requires a template argument";
        return false;
      }
      if (!transform.arguments.empty()) {
        error = "binding transforms do not take arguments";
        return false;
      }
      if (restrictType.has_value()) {
        error = "duplicate restrict transform";
        return false;
      }
      restrictType = *transform.templateArg;
      continue;
    }
    if (isBindingQualifierName(transform.name)) {
      if (transform.templateArg) {
        error = "binding transforms do not take template arguments";
        return false;
      }
      if (!transform.arguments.empty()) {
        error = "binding transforms do not take arguments";
        return false;
      }
      continue;
    }
    if (transform.name == "align_bytes" || transform.name == "align_kbytes") {
      if (!validateAlignTransform(transform, "binding " + expr.name, error)) {
        return false;
      }
      continue;
    }
    if (isPrimitiveBindingTypeName(transform.name)) {
      if (transform.templateArg) {
        error = "binding transforms do not take template arguments";
        return false;
      }
      if (!transform.arguments.empty()) {
        error = "binding transforms do not take arguments";
        return false;
      }
    }
    if (transform.name == "Pointer") {
      if (!transform.templateArg) {
        error = "Pointer requires a template argument";
        return false;
      }
      if (!transform.arguments.empty()) {
        error = "binding transforms do not take arguments";
        return false;
      }
      if (!typeName.empty()) {
        error = "binding requires exactly one type";
        return false;
      }
      typeName = transform.name;
      typeHasTemplate = true;
      info.typeTemplateArg = *transform.templateArg;
      continue;
    }
    if (transform.name == "Reference") {
      if (!transform.templateArg) {
        error = "Reference requires a template argument";
        return false;
      }
      if (!transform.arguments.empty()) {
        error = "binding transforms do not take arguments";
        return false;
      }
      if (!typeName.empty()) {
        error = "binding requires exactly one type";
        return false;
      }
      typeName = transform.name;
      typeHasTemplate = true;
      info.typeTemplateArg = *transform.templateArg;
      continue;
    }
    if (transform.templateArg) {
      if (!transform.arguments.empty()) {
        error = "binding transforms do not take arguments";
        return false;
      }
      if (!typeName.empty()) {
        error = "binding requires exactly one type";
        return false;
      }
      typeName = transform.name;
      typeHasTemplate = true;
      info.typeTemplateArg = *transform.templateArg;
      continue;
    }
    if (!transform.arguments.empty()) {
      error = "binding transforms do not take arguments";
      return false;
    }
    if (!typeName.empty()) {
      error = "binding requires exactly one type";
      return false;
    }
    typeName = transform.name;
  }
  if (typeName.empty()) {
    error = "binding requires a type";
    return false;
  }
  if (!isPrimitiveBindingTypeName(typeName) && !typeHasTemplate) {
    std::string resolved = resolveTypePath(typeName, namespacePrefix);
    if (structTypes.count(resolved) == 0) {
      error = "unsupported binding type: " + typeName;
      return false;
    }
  }
  if (typeHasTemplate && typeName == "Pointer") {
    if (info.typeTemplateArg.empty()) {
      error = "Pointer requires a template argument";
      return false;
    }
    if (!isPrimitiveBindingTypeName(info.typeTemplateArg)) {
      error = "unsupported pointer target type: " + info.typeTemplateArg;
      return false;
    }
  }
  if (typeHasTemplate && typeName == "Reference") {
    if (info.typeTemplateArg.empty()) {
      error = "Reference requires a template argument";
      return false;
    }
    if (!isPrimitiveBindingTypeName(info.typeTemplateArg)) {
      error = "unsupported reference target type: " + info.typeTemplateArg;
      return false;
    }
  }
  if (restrictType.has_value()) {
    if (!restrictMatchesBinding(*restrictType, typeName, info.typeTemplateArg, typeHasTemplate, namespacePrefix)) {
      error = "restrict type does not match binding type";
      return false;
    }
  }
  if (isPrimitiveBindingTypeName(typeName)) {
    info.typeName = typeName;
    return true;
  }
  if (typeHasTemplate) {
    info.typeName = typeName;
    return true;
  }
  info.typeName = typeName;
  return true;
}

bool isAsciiAlpha(char c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

bool isAsciiDigit(char c) {
  return c >= '0' && c <= '9';
}

bool isEffectName(const std::string &text) {
  if (text.empty()) {
    return false;
  }
  if (!isAsciiAlpha(text[0]) && text[0] != '_') {
    return false;
  }
  for (size_t i = 1; i < text.size(); ++i) {
    if (!isAsciiAlpha(text[i]) && !isAsciiDigit(text[i]) && text[i] != '_') {
      return false;
    }
  }
  return true;
}

bool validateEffectsTransform(const Transform &transform, const std::string &context, std::string &error) {
  if (transform.templateArg) {
    error = "effects transform does not accept template arguments on " + context;
    return false;
  }
  std::unordered_set<std::string> seen;
  for (const auto &arg : transform.arguments) {
    if (!isEffectName(arg)) {
      error = "invalid effects capability: " + arg;
      return false;
    }
    if (!seen.insert(arg).second) {
      error = "duplicate effects capability: " + arg;
      return false;
    }
  }
  return true;
}

bool validateCapabilitiesTransform(const Transform &transform, const std::string &context, std::string &error) {
  if (transform.templateArg) {
    error = "capabilities transform does not accept template arguments on " + context;
    return false;
  }
  std::unordered_set<std::string> seen;
  for (const auto &arg : transform.arguments) {
    if (!isEffectName(arg)) {
      error = "invalid capability: " + arg;
      return false;
    }
    if (!seen.insert(arg).second) {
      error = "duplicate capability: " + arg;
      return false;
    }
  }
  return true;
}

bool parsePositiveIntArg(const std::string &text, int &value) {
  std::string digits = text;
  if (digits.size() > 3 && digits.compare(digits.size() - 3, 3, "i32") == 0) {
    digits.resize(digits.size() - 3);
  }
  if (digits.empty()) {
    return false;
  }
  int parsed = 0;
  for (char c : digits) {
    if (!std::isdigit(static_cast<unsigned char>(c))) {
      return false;
    }
    int digit = c - '0';
    if (parsed > (std::numeric_limits<int>::max() - digit) / 10) {
      return false;
    }
    parsed = parsed * 10 + digit;
  }
  if (parsed <= 0) {
    return false;
  }
  value = parsed;
  return true;
}

bool validateAlignTransform(const Transform &transform, const std::string &context, std::string &error) {
  if (transform.templateArg) {
    error = transform.name + " does not accept template arguments on " + context;
    return false;
  }
  if (transform.arguments.size() != 1) {
    error = transform.name + " requires exactly one integer argument";
    return false;
  }
  int value = 0;
  if (!parsePositiveIntArg(transform.arguments[0], value)) {
    error = transform.name + " requires a positive integer argument";
    return false;
  }
  return true;
}

bool isStructTransformName(const std::string &name) {
  return name == "struct" || name == "pod" || name == "stack" || name == "heap" || name == "buffer" ||
         name == "handle" || name == "gpu_lane";
}

bool validateNamedArguments(const std::vector<Expr> &args,
                            const std::vector<std::optional<std::string>> &argNames,
                            const std::string &context,
                            std::string &error) {
  if (!argNames.empty() && argNames.size() != args.size()) {
    error = "argument name count mismatch for " + context;
    return false;
  }
  bool sawNamed = false;
  std::unordered_set<std::string> used;
  for (size_t i = 0; i < args.size(); ++i) {
    if (i < argNames.size() && argNames[i].has_value()) {
      sawNamed = true;
      const std::string &name = *argNames[i];
      if (!used.insert(name).second) {
        error = "duplicate named argument: " + name;
        return false;
      }
      continue;
    }
    if (sawNamed) {
      error = "positional argument cannot follow named arguments";
      return false;
    }
  }
  return true;
}

bool hasNamedArguments(const std::vector<std::optional<std::string>> &argNames) {
  for (const auto &name : argNames) {
    if (name.has_value()) {
      return true;
    }
  }
  return false;
}

bool isDefaultExprAllowed(const Expr &expr) {
  if (expr.isBinding) {
    return false;
  }
  if (expr.kind == Expr::Kind::Name) {
    return false;
  }
  if (expr.kind == Expr::Kind::Call) {
    if (!expr.bodyArguments.empty()) {
      return false;
    }
    for (const auto &arg : expr.args) {
      if (!isDefaultExprAllowed(arg)) {
        return false;
      }
    }
    return true;
  }
  return true;
}

bool buildOrderedArguments(const std::vector<ParameterInfo> &params,
                           const std::vector<Expr> &args,
                           const std::vector<std::optional<std::string>> &argNames,
                           std::vector<const Expr *> &ordered,
                           std::string &error) {
  ordered.assign(params.size(), nullptr);
  size_t positionalIndex = 0;
  for (size_t i = 0; i < args.size(); ++i) {
    if (i < argNames.size() && argNames[i].has_value()) {
      const std::string &name = *argNames[i];
      size_t index = params.size();
      for (size_t p = 0; p < params.size(); ++p) {
        if (params[p].name == name) {
          index = p;
          break;
        }
      }
      if (index >= params.size()) {
        error = "unknown named argument: " + name;
        return false;
      }
      if (ordered[index] != nullptr) {
        error = "named argument duplicates parameter: " + name;
        return false;
      }
      ordered[index] = &args[i];
      continue;
    }
    while (positionalIndex < params.size() && ordered[positionalIndex] != nullptr) {
      ++positionalIndex;
    }
    if (positionalIndex >= params.size()) {
      error = "argument count mismatch";
      return false;
    }
    ordered[positionalIndex] = &args[i];
    ++positionalIndex;
  }
  for (size_t i = 0; i < params.size(); ++i) {
    if (ordered[i] != nullptr) {
      continue;
    }
    if (params[i].defaultExpr) {
      ordered[i] = params[i].defaultExpr;
      continue;
    }
    error = "argument count mismatch";
    return false;
  }
  return true;
}

bool validateNamedArgumentsAgainstParams(const std::vector<ParameterInfo> &params,
                                         const std::vector<std::optional<std::string>> &argNames,
                                         std::string &error) {
  if (argNames.empty()) {
    return true;
  }
  size_t positionalCount = 0;
  while (positionalCount < argNames.size() && !argNames[positionalCount].has_value()) {
    ++positionalCount;
  }
  std::unordered_set<std::string> bound;
  for (size_t i = 0; i < positionalCount && i < params.size(); ++i) {
    bound.insert(params[i].name);
  }
  for (size_t i = positionalCount; i < argNames.size(); ++i) {
    if (!argNames[i].has_value()) {
      continue;
    }
    const std::string &name = *argNames[i];
    bool found = false;
    for (const auto &param : params) {
      if (param.name == name) {
        found = true;
        break;
      }
    }
    if (!found) {
      error = "unknown named argument: " + name;
      return false;
    }
    if (bound.count(name) > 0) {
      error = "named argument duplicates parameter: " + name;
      return false;
    }
    bound.insert(name);
  }
  return true;
}
} // namespace

bool Semantics::validate(const Program &program,
                         const std::string &entryPath,
                         std::string &error,
                         const std::vector<std::string> &defaultEffects) const {
  std::unordered_set<std::string> defaultEffectSet;
  for (const auto &effect : defaultEffects) {
    if (!isEffectName(effect)) {
      error = "invalid default effect: " + effect;
      return false;
    }
    defaultEffectSet.insert(effect);
  }
  std::unordered_map<std::string, const Definition *> defMap;
  std::unordered_map<std::string, ReturnKind> returnKinds;
  std::unordered_set<std::string> structNames;
  std::unordered_map<std::string, std::vector<ParameterInfo>> paramsByDef;
  for (const auto &def : program.definitions) {
    if (defMap.count(def.fullPath) > 0) {
      error = "duplicate definition: " + def.fullPath;
      return false;
    }
    bool sawEffects = false;
    bool sawCapabilities = false;
    for (const auto &transform : def.transforms) {
      if (transform.name == "return") {
        if (!transform.arguments.empty()) {
          error = "return transform does not accept arguments on " + def.fullPath;
          return false;
        }
      } else if (transform.name == "effects") {
        if (sawEffects) {
          error = "duplicate effects transform on " + def.fullPath;
          return false;
        }
        sawEffects = true;
        if (!validateEffectsTransform(transform, def.fullPath, error)) {
          return false;
        }
      } else if (transform.name == "capabilities") {
        if (sawCapabilities) {
          error = "duplicate capabilities transform on " + def.fullPath;
          return false;
        }
        sawCapabilities = true;
        if (!validateCapabilitiesTransform(transform, def.fullPath, error)) {
          return false;
        }
      } else if (transform.name == "align_bytes" || transform.name == "align_kbytes") {
        if (!validateAlignTransform(transform, def.fullPath, error)) {
          return false;
        }
      } else if (isStructTransformName(transform.name)) {
        if (transform.templateArg) {
          error = "struct transform does not accept template arguments on " + def.fullPath;
          return false;
        }
        if (!transform.arguments.empty()) {
          error = "struct transform does not accept arguments on " + def.fullPath;
          return false;
        }
      }
    }
    if (def.name == "Create" || def.name == "Destroy" || def.name == "CreateStack" ||
        def.name == "DestroyStack" || def.name == "CreateHeap" || def.name == "DestroyHeap" ||
        def.name == "CreateBuffer" || def.name == "DestroyBuffer") {
      error = "lifecycle helper must be nested inside a struct: " + def.fullPath;
      return false;
    }
    bool isStruct = false;
    bool hasReturnTransform = false;
    bool isStack = false;
    for (const auto &transform : def.transforms) {
      if (transform.name == "return") {
        hasReturnTransform = true;
      }
      if (isStructTransformName(transform.name)) {
        isStruct = true;
        if (transform.name == "stack") {
          isStack = true;
        }
      }
    }
    bool isFieldOnlyStruct = false;
    if (!isStruct && !hasReturnTransform && def.parameters.empty() && !def.hasReturnStatement &&
        !def.returnExpr.has_value()) {
      isFieldOnlyStruct = true;
      for (const auto &stmt : def.statements) {
        if (!stmt.isBinding) {
          isFieldOnlyStruct = false;
          break;
        }
      }
    }
    if (isStruct || isFieldOnlyStruct) {
      structNames.insert(def.fullPath);
    }
    if (isStruct) {
      if (hasReturnTransform) {
        error = "struct definitions cannot declare return types: " + def.fullPath;
        return false;
      }
      if (def.hasReturnStatement) {
        error = "struct definitions cannot contain return statements: " + def.fullPath;
        return false;
      }
      if (def.returnExpr.has_value()) {
        error = "struct definitions cannot return a value: " + def.fullPath;
        return false;
      }
      if (!def.parameters.empty()) {
        error = "struct definitions cannot declare parameters: " + def.fullPath;
        return false;
      }
      for (const auto &stmt : def.statements) {
        if (!stmt.isBinding) {
          error = "struct definitions may only contain field bindings: " + def.fullPath;
          return false;
        }
        if (isStack && stmt.args.empty()) {
          error = "stack definitions require field initializers: " + def.fullPath;
          return false;
        }
      }
    }
    ReturnKind kind = ReturnKind::Void;
    if (!isStruct) {
      kind = getReturnKind(def, error);
      if (!error.empty()) {
        return false;
      }
    }
    returnKinds[def.fullPath] = kind;
    defMap[def.fullPath] = &def;
  }

  for (const auto &def : program.definitions) {
    std::unordered_set<std::string> seen;
    std::vector<ParameterInfo> params;
    params.reserve(def.parameters.size());
    for (const auto &param : def.parameters) {
      if (!param.isBinding) {
        error = "parameters must use binding syntax: " + def.fullPath;
        return false;
      }
      if (!param.bodyArguments.empty()) {
        error = "parameter does not accept block arguments: " + param.name;
        return false;
      }
      if (!seen.insert(param.name).second) {
        error = "duplicate parameter: " + param.name;
        return false;
      }
      BindingInfo binding;
      if (!parseBindingInfo(param, def.namespacePrefix, structNames, binding, error)) {
        return false;
      }
      if (param.args.size() > 1) {
        error = "parameter defaults accept at most one argument: " + param.name;
        return false;
      }
      if (param.args.size() == 1 && !isDefaultExprAllowed(param.args.front())) {
        error = "parameter default must be a literal or pure expression: " + param.name;
        return false;
      }
      ParameterInfo info;
      info.name = param.name;
      info.binding = std::move(binding);
      if (param.args.size() == 1) {
        info.defaultExpr = &param.args.front();
      }
      params.push_back(std::move(info));
    }
    paramsByDef[def.fullPath] = std::move(params);
  }

  auto resolveCalleePath = [&](const Expr &expr) -> std::string {
    if (expr.name.empty()) {
      return "";
    }
    if (!expr.name.empty() && expr.name[0] == '/') {
      return expr.name;
    }
    if (!expr.namespacePrefix.empty()) {
      return expr.namespacePrefix + "/" + expr.name;
    }
    return "/" + expr.name;
  };

  auto isParam = [](const std::vector<ParameterInfo> &params, const std::string &name) -> bool {
    for (const auto &param : params) {
      if (param.name == name) {
        return true;
      }
    }
    return false;
  };

  auto findParamBinding = [&](const std::vector<ParameterInfo> &params, const std::string &name) -> const BindingInfo * {
    for (const auto &param : params) {
      if (param.name == name) {
        return &param.binding;
      }
    }
    return nullptr;
  };

  std::unordered_set<std::string> inferenceStack;
  std::function<bool(const Definition &)> inferDefinitionReturnKind;
  std::function<ReturnKind(const Expr &,
                           const std::vector<ParameterInfo> &,
                           const std::unordered_map<std::string, BindingInfo> &)>
      inferExprReturnKind;

  inferExprReturnKind = [&](const Expr &expr,
                            const std::vector<ParameterInfo> &params,
                            const std::unordered_map<std::string, BindingInfo> &locals) -> ReturnKind {
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
        if ((left == ReturnKind::Int64 || left == ReturnKind::Int) &&
            (right == ReturnKind::Int64 || right == ReturnKind::Int)) {
          return ReturnKind::Int64;
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
      if (const BindingInfo *paramBinding = findParamBinding(params, expr.name)) {
        if (paramBinding->typeName == "Reference" && !paramBinding->typeTemplateArg.empty()) {
          ReturnKind refKind = returnKindForTypeName(paramBinding->typeTemplateArg);
          if (refKind != ReturnKind::Unknown) {
            return refKind;
          }
        }
        return returnKindForTypeName(paramBinding->typeName);
      }
      auto it = locals.find(expr.name);
      if (it == locals.end()) {
        return ReturnKind::Unknown;
      }
      if (it->second.typeName == "Reference" && !it->second.typeTemplateArg.empty()) {
        ReturnKind refKind = returnKindForTypeName(it->second.typeTemplateArg);
        if (refKind != ReturnKind::Unknown) {
          return refKind;
        }
      }
      return returnKindForTypeName(it->second.typeName);
    }
    if (expr.kind == Expr::Kind::Call) {
      std::string resolved = resolveCalleePath(expr);
      auto defIt = expr.isMethodCall ? defMap.end() : defMap.find(resolved);
      if (defIt != defMap.end()) {
        if (!inferDefinitionReturnKind(*defIt->second)) {
          return ReturnKind::Unknown;
        }
        auto kindIt = returnKinds.find(resolved);
        if (kindIt != returnKinds.end()) {
          return kindIt->second;
        }
        return ReturnKind::Unknown;
      }
      if (expr.isMethodCall && expr.name == "count") {
        return ReturnKind::Int;
      }
      if (!expr.isMethodCall && expr.name == "count" && expr.args.size() == 1 && defMap.find(resolved) == defMap.end()) {
        return ReturnKind::Int;
      }
      auto resolveArrayTarget = [&](const Expr &target, std::string &elemType) -> bool {
        if (target.kind == Expr::Kind::Name) {
          if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
            if (paramBinding->typeName != "array" || paramBinding->typeTemplateArg.empty()) {
              return false;
            }
            elemType = paramBinding->typeTemplateArg;
            return true;
          }
          auto it = locals.find(target.name);
          if (it != locals.end()) {
            if (it->second.typeName != "array" || it->second.typeTemplateArg.empty()) {
              return false;
            }
            elemType = it->second.typeTemplateArg;
            return true;
          }
          return false;
        }
        if (target.kind == Expr::Kind::Call) {
          std::string collection;
          if (getBuiltinCollectionName(target, collection) && collection == "array" && target.templateArgs.size() == 1) {
            elemType = target.templateArgs.front();
            return true;
          }
        }
        return false;
      };
      std::string builtinName;
      if (getBuiltinArrayAccessName(expr, builtinName) && expr.args.size() == 2) {
        std::string elemType;
        if (resolveArrayTarget(expr.args.front(), elemType)) {
          ReturnKind kind = returnKindForTypeName(elemType);
          if (kind != ReturnKind::Unknown) {
            return kind;
          }
        }
        return ReturnKind::Unknown;
      }
      if (getBuiltinComparisonName(expr, builtinName)) {
        return ReturnKind::Bool;
      }
      if (getBuiltinOperatorName(expr, builtinName)) {
        if (builtinName == "negate") {
          if (expr.args.size() != 1) {
            return ReturnKind::Unknown;
          }
          ReturnKind argKind = inferExprReturnKind(expr.args.front(), params, locals);
          if (argKind == ReturnKind::Bool || argKind == ReturnKind::Void) {
            return ReturnKind::Unknown;
          }
          return argKind;
        }
        if (expr.args.size() != 2) {
          return ReturnKind::Unknown;
        }
        ReturnKind left = inferExprReturnKind(expr.args[0], params, locals);
        ReturnKind right = inferExprReturnKind(expr.args[1], params, locals);
        return combineNumeric(left, right);
      }
      if (getBuiltinClampName(expr, builtinName)) {
        if (expr.args.size() != 3) {
          return ReturnKind::Unknown;
        }
        ReturnKind result = inferExprReturnKind(expr.args[0], params, locals);
        result = combineNumeric(result, inferExprReturnKind(expr.args[1], params, locals));
        result = combineNumeric(result, inferExprReturnKind(expr.args[2], params, locals));
        return result;
      }
      if (getBuiltinConvertName(expr, builtinName)) {
        if (expr.templateArgs.size() != 1) {
          return ReturnKind::Unknown;
        }
        return returnKindForTypeName(expr.templateArgs.front());
      }
        if (isAssignCall(expr)) {
        if (expr.args.size() != 2) {
          return ReturnKind::Unknown;
        }
        return inferExprReturnKind(expr.args[1], params, locals);
      }
      return ReturnKind::Unknown;
    }
    return ReturnKind::Unknown;
  };

  inferDefinitionReturnKind = [&](const Definition &def) -> bool {
    auto kindIt = returnKinds.find(def.fullPath);
    if (kindIt == returnKinds.end()) {
      return false;
    }
    if (kindIt->second != ReturnKind::Unknown) {
      return true;
    }
    if (!inferenceStack.insert(def.fullPath).second) {
      error = "return type inference requires explicit annotation on " + def.fullPath;
      return false;
    }
    ReturnKind inferred = ReturnKind::Unknown;
    bool sawReturn = false;
    const auto &defParams = paramsByDef[def.fullPath];
    std::unordered_map<std::string, BindingInfo> locals;
    std::function<bool(const Expr &, std::unordered_map<std::string, BindingInfo> &)> inferStatement;
    inferStatement = [&](const Expr &stmt, std::unordered_map<std::string, BindingInfo> &activeLocals) -> bool {
      if (stmt.isBinding) {
        BindingInfo info;
        if (!parseBindingInfo(stmt, def.namespacePrefix, structNames, info, error)) {
          return false;
        }
        activeLocals.emplace(stmt.name, std::move(info));
        return true;
      }
      if (isReturnCall(stmt)) {
        sawReturn = true;
        ReturnKind exprKind = ReturnKind::Void;
        if (!stmt.args.empty()) {
          exprKind = inferExprReturnKind(stmt.args.front(), defParams, activeLocals);
        }
        if (exprKind == ReturnKind::Unknown) {
          error = "unable to infer return type on " + def.fullPath;
          return false;
        }
        if (inferred == ReturnKind::Unknown) {
          inferred = exprKind;
          return true;
        }
        if (inferred != exprKind) {
          error = "conflicting return types on " + def.fullPath;
          return false;
        }
        return true;
      }
      if (isIfCall(stmt) && stmt.args.size() == 3) {
        const Expr &thenBlock = stmt.args[1];
        const Expr &elseBlock = stmt.args[2];
        auto walkBlock = [&](const Expr &block) -> bool {
          std::unordered_map<std::string, BindingInfo> blockLocals = activeLocals;
          for (const auto &bodyExpr : block.bodyArguments) {
            if (!inferStatement(bodyExpr, blockLocals)) {
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
        return true;
      }
      return true;
    };

    for (const auto &stmt : def.statements) {
      if (!inferStatement(stmt, locals)) {
        return false;
      }
    }
    if (!sawReturn) {
      kindIt->second = ReturnKind::Void;
    } else if (inferred == ReturnKind::Unknown) {
      error = "unable to infer return type on " + def.fullPath;
      return false;
    } else {
      kindIt->second = inferred;
    }
    inferenceStack.erase(def.fullPath);
    return true;
  };

  for (const auto &def : program.definitions) {
    if (returnKinds[def.fullPath] == ReturnKind::Unknown) {
      if (!inferDefinitionReturnKind(def)) {
        return false;
      }
    }
  }

  auto isMutableBinding = [&](const std::vector<ParameterInfo> &params,
                              const std::unordered_map<std::string, BindingInfo> &locals,
                              const std::string &name) -> bool {
    if (const BindingInfo *paramBinding = findParamBinding(params, name)) {
      return paramBinding->isMutable;
    }
    auto it = locals.find(name);
    return it != locals.end() && it->second.isMutable;
  };

  auto returnKindForBinding = [&](const BindingInfo &binding) -> ReturnKind {
    if (binding.typeName == "Reference") {
      return returnKindForTypeName(binding.typeTemplateArg);
    }
    return returnKindForTypeName(binding.typeName);
  };

  auto isIntegerOrBoolExpr = [&](const Expr &arg,
                                 const std::vector<ParameterInfo> &params,
                                 const std::unordered_map<std::string, BindingInfo> &locals) -> bool {
    ReturnKind kind = inferExprReturnKind(arg, params, locals);
    if (kind == ReturnKind::Int || kind == ReturnKind::Int64 || kind == ReturnKind::UInt64 || kind == ReturnKind::Bool) {
      return true;
    }
    if (kind == ReturnKind::Float32 || kind == ReturnKind::Float64 || kind == ReturnKind::Void) {
      return false;
    }
    if (kind == ReturnKind::Unknown) {
      if (arg.kind == Expr::Kind::FloatLiteral || arg.kind == Expr::Kind::StringLiteral) {
        return false;
      }
      if (isPointerExpr(arg, params, locals)) {
        return false;
      }
      if (arg.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(params, arg.name)) {
          ReturnKind paramKind = returnKindForBinding(*paramBinding);
          return paramKind == ReturnKind::Int || paramKind == ReturnKind::Int64 || paramKind == ReturnKind::UInt64 ||
                 paramKind == ReturnKind::Bool;
        }
        auto it = locals.find(arg.name);
        if (it != locals.end()) {
          ReturnKind localKind = returnKindForBinding(it->second);
          return localKind == ReturnKind::Int || localKind == ReturnKind::Int64 || localKind == ReturnKind::UInt64 ||
                 localKind == ReturnKind::Bool;
        }
      }
      return true;
    }
    return false;
  };
  auto isPrintableExpr = [&](const Expr &arg,
                             const std::vector<ParameterInfo> &params,
                             const std::unordered_map<std::string, BindingInfo> &locals) -> bool {
    ReturnKind kind = inferExprReturnKind(arg, params, locals);
    if (kind == ReturnKind::Int || kind == ReturnKind::Int64 || kind == ReturnKind::UInt64 || kind == ReturnKind::Bool ||
        kind == ReturnKind::Float32 || kind == ReturnKind::Float64) {
      return true;
    }
    if (arg.kind == Expr::Kind::StringLiteral) {
      return true;
    }
    if (kind == ReturnKind::Void) {
      return false;
    }
    if (arg.kind == Expr::Kind::Name) {
      if (const BindingInfo *paramBinding = findParamBinding(params, arg.name)) {
        if (paramBinding->typeName == "string") {
          return true;
        }
        ReturnKind paramKind = returnKindForBinding(*paramBinding);
        return paramKind == ReturnKind::Int || paramKind == ReturnKind::Int64 || paramKind == ReturnKind::UInt64 ||
               paramKind == ReturnKind::Bool || paramKind == ReturnKind::Float32 || paramKind == ReturnKind::Float64;
      }
      auto it = locals.find(arg.name);
      if (it != locals.end()) {
        if (it->second.typeName == "string") {
          return true;
        }
        ReturnKind localKind = returnKindForBinding(it->second);
        return localKind == ReturnKind::Int || localKind == ReturnKind::Int64 || localKind == ReturnKind::UInt64 ||
               localKind == ReturnKind::Bool || localKind == ReturnKind::Float32 || localKind == ReturnKind::Float64;
      }
    }
    if (isPointerLikeExpr(arg, params, locals)) {
      return false;
    }
    if (arg.kind == Expr::Kind::Call) {
      std::string builtinName;
      if (getBuiltinCollectionName(arg, builtinName)) {
        return false;
      }
    }
    if (kind == ReturnKind::Unknown) {
      if (arg.kind == Expr::Kind::StringLiteral) {
        return true;
      }
      if (isPointerExpr(arg, params, locals)) {
        return false;
      }
      return true;
    }
    return false;
  };

  std::unordered_set<std::string> activeEffects;

  std::function<bool(const std::vector<ParameterInfo> &, const std::unordered_map<std::string, BindingInfo> &,
                     const Expr &)>
      validateExpr = [&](const std::vector<ParameterInfo> &params,
                         const std::unordered_map<std::string, BindingInfo> &locals,
                         const Expr &expr) -> bool {
    auto isNumericExpr = [&](const Expr &arg) -> bool {
      ReturnKind kind = inferExprReturnKind(arg, params, locals);
      if (kind == ReturnKind::Int || kind == ReturnKind::Int64 || kind == ReturnKind::UInt64 ||
          kind == ReturnKind::Float32 || kind == ReturnKind::Float64) {
        return true;
      }
      if (kind == ReturnKind::Bool || kind == ReturnKind::Void) {
        return false;
      }
      if (kind == ReturnKind::Unknown) {
        if (arg.kind == Expr::Kind::StringLiteral) {
          return false;
        }
        if (isPointerExpr(arg, params, locals)) {
          return false;
        }
        if (arg.kind == Expr::Kind::Name) {
          if (const BindingInfo *paramBinding = findParamBinding(params, arg.name)) {
            ReturnKind paramKind = returnKindForBinding(*paramBinding);
            return paramKind == ReturnKind::Int || paramKind == ReturnKind::Int64 || paramKind == ReturnKind::UInt64 ||
                   paramKind == ReturnKind::Float32 || paramKind == ReturnKind::Float64;
          }
          auto it = locals.find(arg.name);
          if (it != locals.end()) {
            ReturnKind localKind = returnKindForBinding(it->second);
            return localKind == ReturnKind::Int || localKind == ReturnKind::Int64 || localKind == ReturnKind::UInt64 ||
                   localKind == ReturnKind::Float32 || localKind == ReturnKind::Float64;
          }
        }
        return true;
      }
      return false;
    };
    enum class NumericCategory { Unknown, Integer, Float };
    auto numericCategoryForExpr = [&](const Expr &arg) -> NumericCategory {
      ReturnKind kind = inferExprReturnKind(arg, params, locals);
      if (kind == ReturnKind::Float32 || kind == ReturnKind::Float64) {
        return NumericCategory::Float;
      }
      if (kind == ReturnKind::Int || kind == ReturnKind::Int64 || kind == ReturnKind::UInt64 || kind == ReturnKind::Bool) {
        return NumericCategory::Integer;
      }
      if (kind == ReturnKind::Void) {
        return NumericCategory::Unknown;
      }
      if (kind == ReturnKind::Unknown) {
        if (arg.kind == Expr::Kind::FloatLiteral) {
          return NumericCategory::Float;
        }
        if (arg.kind == Expr::Kind::StringLiteral) {
          return NumericCategory::Unknown;
        }
        if (isPointerExpr(arg, params, locals)) {
          return NumericCategory::Unknown;
        }
        if (arg.kind == Expr::Kind::Name) {
          if (const BindingInfo *paramBinding = findParamBinding(params, arg.name)) {
            const std::string &typeName = paramBinding->typeName;
            if (typeName == "float" || typeName == "f32" || typeName == "f64") {
              return NumericCategory::Float;
            }
            if (typeName == "int" || typeName == "i32" || typeName == "i64" || typeName == "u64" ||
                typeName == "bool") {
              return NumericCategory::Integer;
            }
            if (typeName == "Reference") {
              const std::string &refType = paramBinding->typeTemplateArg;
              if (refType == "float" || refType == "f32" || refType == "f64") {
                return NumericCategory::Float;
              }
              if (refType == "int" || refType == "i32" || refType == "i64" || refType == "u64" ||
                  refType == "bool") {
                return NumericCategory::Integer;
              }
            }
          }
          auto it = locals.find(arg.name);
          if (it != locals.end()) {
            const std::string &typeName = it->second.typeName;
            if (typeName == "float" || typeName == "f32" || typeName == "f64") {
              return NumericCategory::Float;
            }
            if (typeName == "int" || typeName == "i32" || typeName == "i64" || typeName == "u64" ||
                typeName == "bool") {
              return NumericCategory::Integer;
            }
            if (typeName == "Reference") {
              const std::string &refType = it->second.typeTemplateArg;
              if (refType == "float" || refType == "f32" || refType == "f64") {
                return NumericCategory::Float;
              }
              if (refType == "int" || refType == "i32" || refType == "i64" || refType == "u64" ||
                  refType == "bool") {
                return NumericCategory::Integer;
              }
            }
          }
        }
        return NumericCategory::Unknown;
      }
      return NumericCategory::Unknown;
    };
    auto hasMixedNumericCategory = [&](const std::vector<Expr> &args) -> bool {
      bool sawFloat = false;
      bool sawInteger = false;
      for (const auto &arg : args) {
        NumericCategory category = numericCategoryForExpr(arg);
        if (category == NumericCategory::Float) {
          sawFloat = true;
        } else if (category == NumericCategory::Integer) {
          sawInteger = true;
        }
      }
      return sawFloat && sawInteger;
    };
    auto isComparisonOperand = [&](const Expr &arg) -> bool {
      ReturnKind kind = inferExprReturnKind(arg, params, locals);
      if (kind == ReturnKind::Bool || kind == ReturnKind::Int || kind == ReturnKind::Int64 || kind == ReturnKind::UInt64 ||
          kind == ReturnKind::Float32 || kind == ReturnKind::Float64) {
        return true;
      }
      if (kind == ReturnKind::Void) {
        return false;
      }
      if (kind == ReturnKind::Unknown) {
        if (arg.kind == Expr::Kind::StringLiteral) {
          return false;
        }
        if (isPointerExpr(arg, params, locals)) {
          return false;
        }
        if (arg.kind == Expr::Kind::Name) {
          if (const BindingInfo *paramBinding = findParamBinding(params, arg.name)) {
            ReturnKind paramKind = returnKindForBinding(*paramBinding);
            return paramKind == ReturnKind::Bool || paramKind == ReturnKind::Int || paramKind == ReturnKind::Int64 ||
                   paramKind == ReturnKind::UInt64 || paramKind == ReturnKind::Float32 || paramKind == ReturnKind::Float64;
          }
          auto it = locals.find(arg.name);
          if (it != locals.end()) {
            ReturnKind localKind = returnKindForBinding(it->second);
            return localKind == ReturnKind::Bool || localKind == ReturnKind::Int || localKind == ReturnKind::Int64 ||
                   localKind == ReturnKind::UInt64 || localKind == ReturnKind::Float32 || localKind == ReturnKind::Float64;
          }
        }
        return true;
      }
      return false;
    };
    auto isUnsignedExpr = [&](const Expr &arg) -> bool {
      ReturnKind kind = inferExprReturnKind(arg, params, locals);
      return kind == ReturnKind::UInt64;
    };
    auto hasMixedSignedness = [&](const std::vector<Expr> &args, bool boolCountsAsSigned) -> bool {
      bool sawUnsigned = false;
      bool sawSigned = false;
      for (const auto &arg : args) {
        ReturnKind kind = inferExprReturnKind(arg, params, locals);
        if (kind == ReturnKind::UInt64) {
          sawUnsigned = true;
          continue;
        }
        if (kind == ReturnKind::Int || kind == ReturnKind::Int64 || (boolCountsAsSigned && kind == ReturnKind::Bool)) {
          sawSigned = true;
        }
      }
      return sawUnsigned && sawSigned;
    };
    if (expr.kind == Expr::Kind::Literal) {
      return true;
    }
    if (expr.kind == Expr::Kind::BoolLiteral) {
      return true;
    }
    if (expr.kind == Expr::Kind::FloatLiteral) {
      return true;
    }
    if (expr.kind == Expr::Kind::StringLiteral) {
      ParsedStringLiteral parsed;
      if (!parseStringLiteralToken(expr.stringValue, parsed, error)) {
        return false;
      }
      if (parsed.encoding == StringEncoding::Ascii && !isAsciiText(parsed.decoded)) {
        error = "ascii string literal contains non-ASCII characters";
        return false;
      }
      return true;
    }
    if (expr.kind == Expr::Kind::Name) {
      if (isParam(params, expr.name) || locals.count(expr.name) > 0) {
        return true;
      }
      error = "unknown identifier: " + expr.name;
      return false;
    }
    if (expr.kind == Expr::Kind::Call) {
      if (expr.isBinding) {
        error = "binding not allowed in expression context";
        return false;
      }
      if (!expr.bodyArguments.empty()) {
        error = "block arguments are only supported on statement calls";
        return false;
      }
      if (isReturnCall(expr)) {
        error = "return not allowed in expression context";
        return false;
      }
      if (isIfCall(expr) || isThenCall(expr) || isElseCall(expr)) {
        error = "control-flow blocks cannot appear in expressions";
        return false;
      }
      auto resolveArrayTarget = [&](const Expr &target, std::string &elemType) -> bool {
        if (target.kind == Expr::Kind::Name) {
          if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
            if (paramBinding->typeName != "array" || paramBinding->typeTemplateArg.empty()) {
              return false;
            }
            elemType = paramBinding->typeTemplateArg;
            return true;
          }
          auto it = locals.find(target.name);
          if (it != locals.end()) {
            if (it->second.typeName != "array" || it->second.typeTemplateArg.empty()) {
              return false;
            }
            elemType = it->second.typeTemplateArg;
            return true;
          }
          return false;
        }
        if (target.kind == Expr::Kind::Call) {
          std::string collection;
          if (getBuiltinCollectionName(target, collection) && collection == "array" && target.templateArgs.size() == 1) {
            elemType = target.templateArgs.front();
            return true;
          }
        }
        return false;
      };
      auto resolveMethodTarget = [&](const Expr &receiver, const std::string &methodName, std::string &resolvedOut) -> bool {
        std::string elemType;
        if (!resolveArrayTarget(receiver, elemType)) {
          error = "unknown method target for " + methodName;
          return false;
        }
        if (methodName == "count") {
          resolvedOut = "/array/count";
          return true;
        }
        error = "unknown method: " + methodName;
        return false;
      };

      std::string resolved = resolveCalleePath(expr);
      bool resolvedMethod = false;
      if (expr.isMethodCall) {
        if (expr.args.empty()) {
          error = "method call missing receiver";
          return false;
        }
        if (!resolveMethodTarget(expr.args.front(), expr.name, resolved)) {
          return false;
        }
        resolvedMethod = true;
      } else if (expr.name == "count" && expr.args.size() == 1 && defMap.find(resolved) == defMap.end()) {
        if (!resolveMethodTarget(expr.args.front(), "count", resolved)) {
          return false;
        }
        resolvedMethod = true;
      }

      if (!validateNamedArguments(expr.args, expr.argNames, resolved, error)) {
        return false;
      }
      auto it = defMap.find(resolved);
      if (it == defMap.end() || resolvedMethod) {
        if (hasNamedArguments(expr.argNames)) {
          error = "named arguments not supported for builtin calls";
          return false;
        }
        if (resolvedMethod && resolved == "/array/count") {
          if (expr.args.size() != 1) {
            error = "argument count mismatch for builtin count";
            return false;
          }
          if (!validateExpr(params, locals, expr.args.front())) {
            return false;
          }
          return true;
        }
        std::string builtinName;
        if (getBuiltinOperatorName(expr, builtinName)) {
          size_t expectedArgs = builtinName == "negate" ? 1 : 2;
          if (expr.args.size() != expectedArgs) {
            error = "argument count mismatch for builtin " + builtinName;
            return false;
          }
          const bool isPlusMinus = builtinName == "plus" || builtinName == "minus";
          bool leftPointer = false;
          bool rightPointer = false;
          if (expr.args.size() == 2) {
            leftPointer = isPointerExpr(expr.args[0], params, locals);
            rightPointer = isPointerExpr(expr.args[1], params, locals);
          }
          if (isPlusMinus && expr.args.size() == 2 && (leftPointer || rightPointer)) {
            if (leftPointer && rightPointer) {
              error = "pointer arithmetic does not support pointer + pointer";
              return false;
            }
            if (rightPointer) {
              error = "pointer arithmetic requires pointer on the left";
              return false;
            }
            const Expr &offsetExpr = expr.args[1];
            ReturnKind offsetKind = inferExprReturnKind(offsetExpr, params, locals);
            if (offsetKind != ReturnKind::Unknown && offsetKind != ReturnKind::Int &&
                offsetKind != ReturnKind::Int64 && offsetKind != ReturnKind::UInt64) {
              error = "pointer arithmetic requires integer offset";
              return false;
            }
          } else if (leftPointer || rightPointer) {
            error = "arithmetic operators require numeric operands";
            return false;
          }
          if (builtinName == "negate") {
            if (!isNumericExpr(expr.args.front())) {
              error = "arithmetic operators require numeric operands";
              return false;
            }
            if (isUnsignedExpr(expr.args.front())) {
              error = "negate does not support unsigned operands";
              return false;
            }
          } else {
            if (!leftPointer && !rightPointer) {
              if (!isNumericExpr(expr.args[0]) || !isNumericExpr(expr.args[1])) {
                error = "arithmetic operators require numeric operands";
                return false;
              }
              if (hasMixedSignedness(expr.args, false)) {
                error = "arithmetic operators do not support mixed signed/unsigned operands";
                return false;
              }
              if (hasMixedNumericCategory(expr.args)) {
                error = "arithmetic operators do not support mixed int/float operands";
                return false;
              }
            } else if (leftPointer) {
              if (!isNumericExpr(expr.args[1])) {
                error = "arithmetic operators require numeric operands";
                return false;
              }
            }
          }
          for (const auto &arg : expr.args) {
            if (!validateExpr(params, locals, arg)) {
              return false;
            }
          }
          return true;
        }
        if (getBuiltinComparisonName(expr, builtinName)) {
          size_t expectedArgs = builtinName == "not" ? 1 : 2;
          if (expr.args.size() != expectedArgs) {
            error = "argument count mismatch for builtin " + builtinName;
            return false;
          }
          const bool isBooleanOp = builtinName == "and" || builtinName == "or" || builtinName == "not";
          for (const auto &arg : expr.args) {
            if (isBooleanOp) {
              if (!isIntegerOrBoolExpr(arg, params, locals)) {
                error = "boolean operators require integer or bool operands";
                return false;
              }
            } else {
              if (!isComparisonOperand(arg)) {
                error = "comparisons require numeric or bool operands";
                return false;
              }
            }
          }
          if (!isBooleanOp) {
            if (hasMixedSignedness(expr.args, true)) {
              error = "comparisons do not support mixed signed/unsigned operands";
              return false;
            }
            if (hasMixedNumericCategory(expr.args)) {
              error = "comparisons do not support mixed int/float operands";
              return false;
            }
          }
          for (const auto &arg : expr.args) {
            if (!validateExpr(params, locals, arg)) {
              return false;
            }
          }
          return true;
        }
        if (getBuiltinClampName(expr, builtinName)) {
          if (expr.args.size() != 3) {
            error = "argument count mismatch for builtin " + builtinName;
            return false;
          }
          for (const auto &arg : expr.args) {
            if (!isNumericExpr(arg)) {
              error = "clamp requires numeric operands";
              return false;
            }
          }
          if (hasMixedSignedness(expr.args, false)) {
            error = "clamp does not support mixed signed/unsigned operands";
            return false;
          }
          if (hasMixedNumericCategory(expr.args)) {
            error = "clamp does not support mixed int/float operands";
            return false;
          }
          for (const auto &arg : expr.args) {
            if (!validateExpr(params, locals, arg)) {
              return false;
            }
          }
          return true;
        }
        if (getBuiltinArrayAccessName(expr, builtinName)) {
          if (expr.args.size() != 2) {
            error = "argument count mismatch for builtin " + builtinName;
            return false;
          }
          std::string elemType;
          if (!resolveArrayTarget(expr.args.front(), elemType)) {
            error = builtinName + " requires array target";
            return false;
          }
          if (!isIntegerOrBoolExpr(expr.args[1], params, locals)) {
            error = builtinName + " requires integer index";
            return false;
          }
          for (const auto &arg : expr.args) {
            if (!validateExpr(params, locals, arg)) {
              return false;
            }
          }
          return true;
        }
        if (getBuiltinConvertName(expr, builtinName)) {
          if (expr.templateArgs.size() != 1) {
            error = "convert requires exactly one template argument";
            return false;
          }
          const std::string &typeName = expr.templateArgs[0];
          if (typeName != "int" && typeName != "i32" && typeName != "i64" && typeName != "u64" &&
              typeName != "bool" && typeName != "float" && typeName != "f32" && typeName != "f64") {
            error = "unsupported convert target type: " + typeName;
            return false;
          }
          if (expr.args.size() != 1) {
            error = "argument count mismatch for builtin " + builtinName;
            return false;
          }
          if (!validateExpr(params, locals, expr.args.front())) {
            return false;
          }
          return true;
        }
        PrintBuiltin printBuiltin;
        if (getPrintBuiltin(expr, printBuiltin)) {
          error = printBuiltin.name + " is only supported as a statement";
          return false;
        }
        if (getBuiltinPointerName(expr, builtinName)) {
          if (!expr.templateArgs.empty()) {
            error = "pointer helpers do not accept template arguments";
            return false;
          }
          if (expr.args.size() != 1) {
            error = "argument count mismatch for builtin " + builtinName;
            return false;
          }
          if (builtinName == "location") {
            const Expr &target = expr.args.front();
            if (target.kind != Expr::Kind::Name || locals.count(target.name) == 0 || isParam(params, target.name)) {
              error = "location requires a local binding";
              return false;
            }
          }
          if (builtinName == "dereference") {
            if (!isPointerLikeExpr(expr.args.front(), params, locals)) {
              error = "dereference requires a pointer or reference";
              return false;
            }
          }
          if (!validateExpr(params, locals, expr.args.front())) {
            return false;
          }
          return true;
        }
        if (getBuiltinCollectionName(expr, builtinName)) {
          if (builtinName == "array") {
            if (expr.templateArgs.size() != 1) {
              error = "array literal requires exactly one template argument";
              return false;
            }
          } else {
            if (expr.templateArgs.size() != 2) {
              error = "map literal requires exactly two template arguments";
              return false;
            }
            if (expr.args.size() % 2 != 0) {
              error = "map literal requires an even number of arguments";
              return false;
            }
          }
          for (const auto &arg : expr.args) {
            if (!validateExpr(params, locals, arg)) {
              return false;
            }
          }
          return true;
        }
        if (isAssignCall(expr)) {
          if (expr.args.size() != 2) {
            error = "assign requires exactly two arguments";
            return false;
          }
          const Expr &target = expr.args.front();
          if (target.kind == Expr::Kind::Name) {
            if (!isMutableBinding(params, locals, target.name)) {
              error = "assign target must be a mutable binding: " + target.name;
              return false;
            }
          } else if (target.kind == Expr::Kind::Call) {
            std::string pointerName;
            if (!getBuiltinPointerName(target, pointerName) || pointerName != "dereference" ||
                target.args.size() != 1) {
              error = "assign target must be a mutable binding";
              return false;
            }
            const Expr &pointerExpr = target.args.front();
            if (pointerExpr.kind != Expr::Kind::Name || !isMutableBinding(params, locals, pointerExpr.name)) {
              error = "assign target must be a mutable binding";
              return false;
            }
            if (!isPointerLikeExpr(pointerExpr, params, locals)) {
              error = "assign target must be a mutable pointer binding";
              return false;
            }
            if (!validateExpr(params, locals, pointerExpr)) {
              return false;
            }
          } else {
            error = "assign target must be a mutable binding";
            return false;
          }
          if (!validateExpr(params, locals, expr.args[1])) {
            return false;
          }
          return true;
        }
        error = "unknown call target: " + expr.name;
        return false;
      }
      const auto &calleeParams = paramsByDef[resolved];
      if (!validateNamedArgumentsAgainstParams(calleeParams, expr.argNames, error)) {
        return false;
      }
      std::vector<const Expr *> orderedArgs;
      std::string orderError;
      if (!buildOrderedArguments(calleeParams, expr.args, expr.argNames, orderedArgs, orderError)) {
        if (orderError.find("argument count mismatch") != std::string::npos) {
          error = "argument count mismatch for " + resolved;
        } else {
          error = orderError;
        }
        return false;
      }
      for (const auto *arg : orderedArgs) {
        if (!arg) {
          continue;
        }
        if (!validateExpr(params, locals, *arg)) {
          return false;
        }
      }
      return true;
    }
    return false;
  };

  std::function<bool(const std::vector<ParameterInfo> &,
                     std::unordered_map<std::string, BindingInfo> &,
                     const Expr &,
                     ReturnKind,
                     bool,
                     bool,
                     bool *,
                     const std::string &)>
      validateStatement = [&](const std::vector<ParameterInfo> &params,
                              std::unordered_map<std::string, BindingInfo> &locals,
                              const Expr &stmt,
                              ReturnKind returnKind,
                              bool allowReturn,
                              bool allowBindings,
                              bool *sawReturn,
                              const std::string &namespacePrefix) -> bool {
    if (stmt.isBinding) {
      if (!allowBindings) {
        error = "binding not allowed in execution body";
        return false;
      }
      if (!stmt.bodyArguments.empty()) {
        error = "binding does not accept block arguments";
        return false;
      }
      if (isParam(params, stmt.name) || locals.count(stmt.name) > 0) {
        error = "duplicate binding name: " + stmt.name;
        return false;
      }
      BindingInfo info;
      if (!parseBindingInfo(stmt, namespacePrefix, structNames, info, error)) {
        return false;
      }
      if (stmt.args.size() != 1) {
        error = "binding requires exactly one argument";
        return false;
      }
      if (!validateExpr(params, locals, stmt.args.front())) {
        return false;
      }
      if (info.typeName == "Reference") {
        const Expr &init = stmt.args.front();
        std::string pointerName;
        if (init.kind != Expr::Kind::Call || !getBuiltinPointerName(init, pointerName) ||
            pointerName != "location" || init.args.size() != 1) {
          error = "Reference bindings require location(...)";
          return false;
        }
      }
      locals.emplace(stmt.name, info);
      return true;
    }
    if (stmt.kind != Expr::Kind::Call) {
      if (!allowBindings) {
        error = "execution body arguments must be calls";
        return false;
      }
      return validateExpr(params, locals, stmt);
    }
    if (isReturnCall(stmt)) {
      if (!allowReturn) {
        error = "return not allowed in execution body";
        return false;
      }
      if (!stmt.bodyArguments.empty()) {
        error = "return does not accept block arguments";
        return false;
      }
      if (returnKind == ReturnKind::Void) {
        if (!stmt.args.empty()) {
          error = "return value not allowed for void definition";
          return false;
        }
      } else {
        if (stmt.args.size() != 1) {
          error = "return requires exactly one argument";
          return false;
        }
        if (!validateExpr(params, locals, stmt.args.front())) {
          return false;
        }
      }
      if (sawReturn) {
        *sawReturn = true;
      }
      return true;
    }
    if (isIfCall(stmt)) {
      if (!stmt.bodyArguments.empty()) {
        error = "if does not accept trailing block arguments";
        return false;
      }
      if (stmt.args.size() != 3) {
        error = "if requires condition, then, else";
        return false;
      }
      const Expr &cond = stmt.args[0];
      const Expr &thenBlock = stmt.args[1];
      const Expr &elseBlock = stmt.args[2];
      if (!validateExpr(params, locals, cond)) {
        return false;
      }
      if (!isIntegerOrBoolExpr(cond, params, locals)) {
        error = "if condition requires integer or bool";
        return false;
      }
      auto validateBlock = [&](const Expr &block, const char *label) -> bool {
        if (block.kind != Expr::Kind::Call) {
          error = std::string(label) + " must be a call";
          return false;
        }
        if ((std::string(label) == "then" && !isThenCall(block)) ||
            (std::string(label) == "else" && !isElseCall(block))) {
          error = std::string(label) + " must use the " + label + " wrapper";
          return false;
        }
        if (!block.args.empty()) {
          error = std::string(label) + " does not accept arguments";
          return false;
        }
        std::unordered_map<std::string, BindingInfo> blockLocals = locals;
        for (const auto &bodyExpr : block.bodyArguments) {
          if (!validateStatement(params,
                                 blockLocals,
                                 bodyExpr,
                                 returnKind,
                                 allowReturn,
                                 allowBindings,
                                 sawReturn,
                                 namespacePrefix)) {
            return false;
          }
        }
        return true;
      };
      if (!validateBlock(thenBlock, "then")) {
        return false;
      }
      if (!validateBlock(elseBlock, "else")) {
        return false;
      }
      return true;
    }
    PrintBuiltin printBuiltin;
    if (getPrintBuiltin(stmt, printBuiltin)) {
      if (hasNamedArguments(stmt.argNames)) {
        error = "named arguments not supported for builtin calls";
        return false;
      }
      if (!stmt.bodyArguments.empty()) {
        error = printBuiltin.name + " does not accept block arguments";
        return false;
      }
      if (stmt.args.size() != 1) {
        error = printBuiltin.name + " requires exactly one argument";
        return false;
      }
      const std::string effectName = (printBuiltin.target == PrintTarget::Err) ? "io_err" : "io_out";
      if (activeEffects.count(effectName) == 0) {
        error = printBuiltin.name + " requires " + effectName + " effect";
        return false;
      }
      if (!validateExpr(params, locals, stmt.args.front())) {
        return false;
      }
      if (!isPrintableExpr(stmt.args.front(), params, locals)) {
        error = printBuiltin.name + " requires a numeric/bool or string literal/binding argument";
        return false;
      }
      return true;
    }
    if (!stmt.bodyArguments.empty()) {
      if (isThenCall(stmt) || isElseCall(stmt)) {
        error = "then/else blocks must be nested inside if";
        return false;
      }
      std::string resolved = resolveCalleePath(stmt);
      if (defMap.count(resolved) == 0) {
        error = "block arguments require a definition target: " + resolved;
        return false;
      }
      Expr call = stmt;
      call.bodyArguments.clear();
      if (!validateExpr(params, locals, call)) {
        return false;
      }
      std::unordered_map<std::string, BindingInfo> blockLocals = locals;
      for (const auto &bodyExpr : stmt.bodyArguments) {
        if (!validateStatement(params, blockLocals, bodyExpr, returnKind, false, false, nullptr, namespacePrefix)) {
          return false;
        }
      }
      return true;
    }
    return validateExpr(params, locals, stmt);
  };

  std::function<bool(const std::vector<Expr> &)> blockAlwaysReturns;
  std::function<bool(const Expr &)> statementAlwaysReturns;
  statementAlwaysReturns = [&](const Expr &stmt) -> bool {
    if (isReturnCall(stmt)) {
      return true;
    }
    if (isIfCall(stmt) && stmt.args.size() == 3) {
      const Expr &thenBlock = stmt.args[1];
      const Expr &elseBlock = stmt.args[2];
      if (!isThenCall(thenBlock) || !isElseCall(elseBlock)) {
        return false;
      }
      return blockAlwaysReturns(thenBlock.bodyArguments) && blockAlwaysReturns(elseBlock.bodyArguments);
    }
    return false;
  };
  blockAlwaysReturns = [&](const std::vector<Expr> &statements) -> bool {
    for (const auto &stmt : statements) {
      if (statementAlwaysReturns(stmt)) {
        return true;
      }
    }
    return false;
  };

  auto resolveEffects = [&](const std::vector<Transform> &transforms) {
    bool sawEffects = false;
    std::unordered_set<std::string> effects;
    for (const auto &transform : transforms) {
      if (transform.name != "effects") {
        continue;
      }
      sawEffects = true;
      effects.clear();
      for (const auto &arg : transform.arguments) {
        effects.insert(arg);
      }
    }
    if (!sawEffects) {
      effects = defaultEffectSet;
    }
    return effects;
  };
  auto validateCapabilitiesSubset = [&](const std::vector<Transform> &transforms, const std::string &context) -> bool {
    bool sawCapabilities = false;
    std::unordered_set<std::string> capabilities;
    for (const auto &transform : transforms) {
      if (transform.name != "capabilities") {
        continue;
      }
      sawCapabilities = true;
      capabilities.clear();
      for (const auto &arg : transform.arguments) {
        capabilities.insert(arg);
      }
    }
    if (!sawCapabilities) {
      return true;
    }
    for (const auto &capability : capabilities) {
      if (activeEffects.count(capability) == 0) {
        error = "capability requires matching effect on " + context + ": " + capability;
        return false;
      }
    }
    return true;
  };

  for (const auto &def : program.definitions) {
    activeEffects = resolveEffects(def.transforms);
    if (!validateCapabilitiesSubset(def.transforms, def.fullPath)) {
      return false;
    }
    std::unordered_map<std::string, BindingInfo> locals;
    const auto &defParams = paramsByDef[def.fullPath];
    for (const auto &param : defParams) {
      locals.emplace(param.name, param.binding);
    }
    ReturnKind kind = ReturnKind::Unknown;
    auto kindIt = returnKinds.find(def.fullPath);
    if (kindIt != returnKinds.end()) {
      kind = kindIt->second;
    }
    bool sawReturn = false;
    for (const auto &stmt : def.statements) {
      if (!validateStatement(defParams, locals, stmt, kind, true, true, &sawReturn, def.namespacePrefix)) {
        return false;
      }
    }
    if (kind != ReturnKind::Void) {
      bool allPathsReturn = blockAlwaysReturns(def.statements);
      if (!allPathsReturn) {
        if (sawReturn) {
          error = "not all control paths return in " + def.fullPath;
        } else {
          error = "missing return statement in " + def.fullPath;
        }
        return false;
      }
    }
  }

  for (const auto &exec : program.executions) {
    activeEffects = resolveEffects(exec.transforms);
    bool sawEffects = false;
    bool sawCapabilities = false;
    for (const auto &transform : exec.transforms) {
      if (transform.name == "return") {
        error = "return transform not allowed on executions: " + exec.fullPath;
        return false;
      }
      if (transform.name == "effects") {
        if (sawEffects) {
          error = "duplicate effects transform on " + exec.fullPath;
          return false;
        }
        sawEffects = true;
        if (!validateEffectsTransform(transform, exec.fullPath, error)) {
          return false;
        }
      } else if (transform.name == "capabilities") {
        if (sawCapabilities) {
          error = "duplicate capabilities transform on " + exec.fullPath;
          return false;
        }
        sawCapabilities = true;
        if (!validateCapabilitiesTransform(transform, exec.fullPath, error)) {
          return false;
        }
      } else if (transform.name == "align_bytes" || transform.name == "align_kbytes") {
        if (!validateAlignTransform(transform, exec.fullPath, error)) {
          return false;
        }
      } else if (isStructTransformName(transform.name)) {
        error = "struct transforms are not allowed on executions: " + exec.fullPath;
        return false;
      }
    }
    if (!validateCapabilitiesSubset(exec.transforms, exec.fullPath)) {
      return false;
    }
    auto it = defMap.find(exec.fullPath);
    if (it == defMap.end()) {
      error = "unknown execution target: " + exec.fullPath;
      return false;
    }
    if (!validateNamedArguments(exec.arguments, exec.argumentNames, exec.fullPath, error)) {
      return false;
    }
    const auto &execParams = paramsByDef[exec.fullPath];
    if (!validateNamedArgumentsAgainstParams(execParams, exec.argumentNames, error)) {
      return false;
    }
    std::vector<const Expr *> orderedExecArgs;
    std::string orderError;
    if (!buildOrderedArguments(execParams, exec.arguments, exec.argumentNames, orderedExecArgs, orderError)) {
      if (orderError.find("argument count mismatch") != std::string::npos) {
        error = "argument count mismatch for " + exec.fullPath;
      } else {
        error = orderError;
      }
      return false;
    }
    for (const auto *arg : orderedExecArgs) {
      if (!arg) {
        continue;
      }
      if (!validateExpr({}, std::unordered_map<std::string, BindingInfo>{}, *arg)) {
        return false;
      }
    }
    std::unordered_map<std::string, BindingInfo> execLocals;
    for (const auto &arg : exec.bodyArguments) {
      if (arg.kind != Expr::Kind::Call) {
        error = "execution body arguments must be calls";
        return false;
      }
      if (arg.isBinding) {
        error = "execution body arguments cannot be bindings";
        return false;
      }
      if (!validateStatement({}, execLocals, arg, ReturnKind::Unknown, false, false, nullptr, exec.namespacePrefix)) {
        return false;
      }
    }
  }

  auto entryIt = defMap.find(entryPath);
  if (entryIt == defMap.end()) {
    error = "missing entry definition " + entryPath;
    return false;
  }
  const auto &entryParams = paramsByDef[entryPath];
  if (!entryParams.empty()) {
    if (entryParams.size() != 1) {
      error = "entry definition must take a single array<string> parameter: " + entryPath;
      return false;
    }
    const ParameterInfo &param = entryParams.front();
    if (param.binding.typeName != "array" || param.binding.typeTemplateArg != "string") {
      error = "entry definition must take a single array<string> parameter: " + entryPath;
      return false;
    }
    if (param.defaultExpr != nullptr) {
      error = "entry parameter does not allow a default value: " + entryPath;
      return false;
    }
  }

  return true;
}

} // namespace primec
