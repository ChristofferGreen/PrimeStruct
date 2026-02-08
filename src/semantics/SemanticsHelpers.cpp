#include "SemanticsHelpers.h"

#include <cctype>
#include <functional>
#include <limits>

namespace primec::semantics {

bool validateAlignTransform(const Transform &transform, const std::string &context, std::string &error);
std::string resolveTypePath(const std::string &name, const std::string &namespacePrefix);

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

std::string joinTemplateArgs(const std::vector<std::string> &args) {
  std::string out;
  for (size_t i = 0; i < args.size(); ++i) {
    if (i > 0) {
      out += ", ";
    }
    out += args[i];
  }
  return out;
}

bool splitTopLevelTemplateArgs(const std::string &text, std::vector<std::string> &out) {
  out.clear();
  int depth = 0;
  size_t start = 0;
  auto pushSegment = [&](size_t end) {
    size_t segStart = start;
    while (segStart < end && std::isspace(static_cast<unsigned char>(text[segStart]))) {
      ++segStart;
    }
    size_t segEnd = end;
    while (segEnd > segStart && std::isspace(static_cast<unsigned char>(text[segEnd - 1]))) {
      --segEnd;
    }
    out.push_back(text.substr(segStart, segEnd - segStart));
  };
  for (size_t i = 0; i < text.size(); ++i) {
    char c = text[i];
    if (c == '<') {
      ++depth;
      continue;
    }
    if (c == '>') {
      if (depth > 0) {
        --depth;
      }
      continue;
    }
    if (c == ',' && depth == 0) {
      pushSegment(i);
      start = i + 1;
    }
  }
  pushSegment(text.size());
  for (const auto &seg : out) {
    if (seg.empty()) {
      return false;
    }
  }
  return !out.empty();
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
    if (transform.templateArgs.size() != 1) {
      error = "return transform requires a type on " + def.fullPath;
      return ReturnKind::Unknown;
    }
    ReturnKind nextKind = returnKindForTypeName(transform.templateArgs.front());
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

bool isReturnCall(const Expr &expr) {
  return isSimpleCallName(expr, "return");
}

bool isRepeatCall(const Expr &expr) {
  return isSimpleCallName(expr, "repeat");
}

bool isBlockCall(const Expr &expr) {
  return isSimpleCallName(expr, "block");
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

bool getPathSpaceBuiltin(const Expr &expr, PathSpaceBuiltin &out) {
  if (isSimpleCallName(expr, "notify")) {
    out.target = PathSpaceTarget::Notify;
    out.name = "notify";
    out.requiredEffect = "pathspace_notify";
    out.argumentCount = 2;
    return true;
  }
  if (isSimpleCallName(expr, "insert")) {
    out.target = PathSpaceTarget::Insert;
    out.name = "insert";
    out.requiredEffect = "pathspace_insert";
    out.argumentCount = 2;
    return true;
  }
  if (isSimpleCallName(expr, "take")) {
    out.target = PathSpaceTarget::Take;
    out.name = "take";
    out.requiredEffect = "pathspace_take";
    out.argumentCount = 1;
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
                      std::optional<std::string> &restrictTypeOut,
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
      if (!transform.templateArgs.empty()) {
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
      if (!transform.templateArgs.empty()) {
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
      if (transform.templateArgs.size() != 1) {
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
      restrictType = transform.templateArgs.front();
      continue;
    }
    if (isBindingQualifierName(transform.name)) {
      if (!transform.templateArgs.empty()) {
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
      if (!transform.templateArgs.empty()) {
        error = "binding transforms do not take template arguments";
        return false;
      }
      if (!transform.arguments.empty()) {
        error = "binding transforms do not take arguments";
        return false;
      }
    }
    if (transform.name == "Pointer") {
      if (transform.templateArgs.size() != 1) {
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
      info.typeTemplateArg = transform.templateArgs.front();
      continue;
    }
    if (transform.name == "Reference") {
      if (transform.templateArgs.size() != 1) {
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
      info.typeTemplateArg = transform.templateArgs.front();
      continue;
    }
    if (!transform.templateArgs.empty()) {
      if (!transform.arguments.empty()) {
        error = "binding transforms do not take arguments";
        return false;
      }
      if (!typeName.empty()) {
        error = "binding requires exactly one type";
        return false;
      }
      if (transform.name == "array" && transform.templateArgs.size() != 1) {
        error = "array requires exactly one template argument";
        return false;
      }
      if (transform.name == "map" && transform.templateArgs.size() != 2) {
        error = "map requires exactly two template arguments";
        return false;
      }
      typeName = transform.name;
      typeHasTemplate = true;
      info.typeTemplateArg = joinTemplateArgs(transform.templateArgs);
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
    typeName = "int";
  }
  restrictTypeOut = restrictType;
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
  if (!transform.templateArgs.empty()) {
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
  if (!transform.templateArgs.empty()) {
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
  if (!transform.templateArgs.empty()) {
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
    if (hasNamedArguments(expr.argNames)) {
      return false;
    }
    if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
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

std::string inferPrimitiveBindingTypeFromInitializer(const Expr &expr) {
  if (expr.kind == Expr::Kind::Literal) {
    if (expr.isUnsigned) {
      return "u64";
    }
    return expr.intWidth == 64 ? "i64" : "i32";
  }
  if (expr.kind == Expr::Kind::BoolLiteral) {
    return "bool";
  }
  if (expr.kind == Expr::Kind::FloatLiteral) {
    return expr.floatWidth == 64 ? "f64" : "f32";
  }
  std::string builtinName;
  if (expr.kind == Expr::Kind::Call && getBuiltinConvertName(expr, builtinName) && expr.templateArgs.size() == 1) {
    const std::string &typeName = expr.templateArgs.front();
    if (isPrimitiveBindingTypeName(typeName)) {
      return typeName;
    }
  }
  return "";
}

bool tryInferBindingTypeFromInitializer(const Expr &initializer,
                                        const std::vector<ParameterInfo> &params,
                                        const std::unordered_map<std::string, BindingInfo> &locals,
                                        BindingInfo &bindingOut) {
  const auto findParamBinding = [&](const std::string &name) -> const BindingInfo * {
    for (const auto &param : params) {
      if (param.name == name) {
        return &param.binding;
      }
    }
    return nullptr;
  };

  std::string inferredPrimitive = inferPrimitiveBindingTypeFromInitializer(initializer);
  if (!inferredPrimitive.empty()) {
    bindingOut.typeName = inferredPrimitive;
    bindingOut.typeTemplateArg.clear();
    return true;
  }
  if (initializer.kind == Expr::Kind::Name) {
    const std::string &name = initializer.name;
    if (const BindingInfo *paramBinding = findParamBinding(name)) {
      bindingOut.typeName = paramBinding->typeName;
      bindingOut.typeTemplateArg = paramBinding->typeTemplateArg;
      return true;
    }
    auto it = locals.find(name);
    if (it != locals.end()) {
      bindingOut.typeName = it->second.typeName;
      bindingOut.typeTemplateArg = it->second.typeTemplateArg;
      return true;
    }
    return false;
  }
  if (initializer.kind == Expr::Kind::Call) {
    std::string collection;
    if (getBuiltinCollectionName(initializer, collection)) {
      if (collection == "array" && initializer.templateArgs.size() == 1) {
        bindingOut.typeName = "array";
        bindingOut.typeTemplateArg = initializer.templateArgs.front();
        return true;
      }
      if (collection == "map" && initializer.templateArgs.size() == 2) {
        bindingOut.typeName = "map";
        bindingOut.typeTemplateArg = joinTemplateArgs(initializer.templateArgs);
        return true;
      }
    }
  }
  const auto typeNameForKind = [](ReturnKind kind) -> std::string {
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
  };
  const auto combineNumeric = [](ReturnKind left, ReturnKind right) -> ReturnKind {
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
      if ((left == ReturnKind::Int64 || left == ReturnKind::Int) && (right == ReturnKind::Int64 || right == ReturnKind::Int)) {
        return ReturnKind::Int64;
      }
      return ReturnKind::Unknown;
    }
    if (left == ReturnKind::Int && right == ReturnKind::Int) {
      return ReturnKind::Int;
    }
    return ReturnKind::Unknown;
  };

  std::function<ReturnKind(const Expr &)> inferPrimitiveReturnKind;
  inferPrimitiveReturnKind = [&](const Expr &expr) -> ReturnKind {
    std::string inferred = inferPrimitiveBindingTypeFromInitializer(expr);
    if (!inferred.empty()) {
      return returnKindForTypeName(normalizeBindingTypeName(inferred));
    }
    if (expr.kind == Expr::Kind::Name) {
      const BindingInfo *binding = findParamBinding(expr.name);
      if (!binding) {
        auto it = locals.find(expr.name);
        if (it != locals.end()) {
          binding = &it->second;
        }
      }
      if (!binding) {
        return ReturnKind::Unknown;
      }
      if (binding->typeName == "Reference" && !binding->typeTemplateArg.empty()) {
        ReturnKind kind = returnKindForTypeName(normalizeBindingTypeName(binding->typeTemplateArg));
        return kind == ReturnKind::Unknown ? ReturnKind::Unknown : kind;
      }
      ReturnKind kind = returnKindForTypeName(normalizeBindingTypeName(binding->typeName));
      return kind == ReturnKind::Unknown ? ReturnKind::Unknown : kind;
    }
    if (expr.kind == Expr::Kind::Call) {
      if (expr.isMethodCall) {
        return ReturnKind::Unknown;
      }
      if (expr.hasBodyArguments && expr.args.empty()) {
        ReturnKind last = ReturnKind::Unknown;
        bool sawValue = false;
        for (const auto &bodyExpr : expr.bodyArguments) {
          if (bodyExpr.isBinding) {
            continue;
          }
          sawValue = true;
          last = inferPrimitiveReturnKind(bodyExpr);
        }
        return sawValue ? last : ReturnKind::Unknown;
      }
      std::string builtinName;
      if (getBuiltinComparisonName(expr, builtinName)) {
        return ReturnKind::Bool;
      }
      if (getBuiltinOperatorName(expr, builtinName)) {
        if (builtinName == "negate") {
          if (expr.args.size() != 1) {
            return ReturnKind::Unknown;
          }
          ReturnKind argKind = inferPrimitiveReturnKind(expr.args.front());
          if (argKind == ReturnKind::Bool || argKind == ReturnKind::Void) {
            return ReturnKind::Unknown;
          }
          return argKind;
        }
        if (expr.args.size() != 2) {
          return ReturnKind::Unknown;
        }
        ReturnKind left = inferPrimitiveReturnKind(expr.args[0]);
        ReturnKind right = inferPrimitiveReturnKind(expr.args[1]);
        return combineNumeric(left, right);
      }
      if (getBuiltinClampName(expr, builtinName)) {
        if (expr.args.size() != 3) {
          return ReturnKind::Unknown;
        }
        ReturnKind result = inferPrimitiveReturnKind(expr.args[0]);
        result = combineNumeric(result, inferPrimitiveReturnKind(expr.args[1]));
        result = combineNumeric(result, inferPrimitiveReturnKind(expr.args[2]));
        return result;
      }
      if (getBuiltinMinMaxName(expr, builtinName)) {
        if (expr.args.size() != 2) {
          return ReturnKind::Unknown;
        }
        ReturnKind result = inferPrimitiveReturnKind(expr.args[0]);
        result = combineNumeric(result, inferPrimitiveReturnKind(expr.args[1]));
        return result;
      }
      if (getBuiltinAbsSignName(expr, builtinName)) {
        if (expr.args.size() != 1) {
          return ReturnKind::Unknown;
        }
        ReturnKind argKind = inferPrimitiveReturnKind(expr.args.front());
        if (argKind == ReturnKind::Bool || argKind == ReturnKind::Void) {
          return ReturnKind::Unknown;
        }
        return argKind;
      }
      if (getBuiltinSaturateName(expr, builtinName)) {
        if (expr.args.size() != 1) {
          return ReturnKind::Unknown;
        }
        ReturnKind argKind = inferPrimitiveReturnKind(expr.args.front());
        if (argKind == ReturnKind::Bool || argKind == ReturnKind::Void) {
          return ReturnKind::Unknown;
        }
        return argKind;
      }
      if (getBuiltinConvertName(expr, builtinName)) {
        if (expr.templateArgs.size() != 1) {
          return ReturnKind::Unknown;
        }
        ReturnKind kind = returnKindForTypeName(normalizeBindingTypeName(expr.templateArgs.front()));
        return kind == ReturnKind::Unknown ? ReturnKind::Unknown : kind;
      }
      if (isAssignCall(expr)) {
        if (expr.args.size() != 2) {
          return ReturnKind::Unknown;
        }
        return inferPrimitiveReturnKind(expr.args[1]);
      }
      if (isIfCall(expr)) {
        if (expr.args.size() != 3) {
          return ReturnKind::Unknown;
        }
        ReturnKind thenKind = inferPrimitiveReturnKind(expr.args[1]);
        ReturnKind elseKind = inferPrimitiveReturnKind(expr.args[2]);
        if (thenKind == elseKind) {
          return thenKind;
        }
        return combineNumeric(thenKind, elseKind);
      }
      return ReturnKind::Unknown;
    }
    return ReturnKind::Unknown;
  };

  ReturnKind inferredKind = inferPrimitiveReturnKind(initializer);
  std::string inferredType = typeNameForKind(inferredKind);
  if (!inferredType.empty()) {
    bindingOut.typeName = inferredType;
    bindingOut.typeTemplateArg.clear();
    return true;
  }
  return false;
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

} // namespace primec::semantics
