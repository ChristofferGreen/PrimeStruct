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

bool isSoftwareNumericTypeName(const std::string &name) {
  return name == "integer" || name == "decimal" || name == "complex";
}

std::optional<std::string> findSoftwareNumericType(const std::string &typeName) {
  std::string base;
  std::string arg;
  if (!splitTemplateTypeName(typeName, base, arg)) {
    if (isSoftwareNumericTypeName(normalizeBindingTypeName(typeName))) {
      return normalizeBindingTypeName(typeName);
    }
    return std::nullopt;
  }
  if (isSoftwareNumericTypeName(normalizeBindingTypeName(base))) {
    return normalizeBindingTypeName(base);
  }
  std::vector<std::string> args;
  if (!splitTopLevelTemplateArgs(arg, args)) {
    return std::nullopt;
  }
  for (const auto &nested : args) {
    if (auto found = findSoftwareNumericType(nested)) {
      return found;
    }
  }
  return std::nullopt;
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
    if (auto softwareType = findSoftwareNumericType(transform.templateArgs.front())) {
      error = "software numeric types are not supported yet: " + *softwareType;
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
  std::optional<std::string> visibilityTransform;
  bool sawStatic = false;
  for (const auto &transform : expr.transforms) {
    if (transform.name == "effects" || transform.name == "capabilities") {
      error = "binding does not accept " + transform.name + " transform";
      return false;
    }
    if (transform.name == "return") {
      error = "binding does not accept return transform";
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
      if (transform.name == "static") {
        if (sawStatic) {
          error = "duplicate static transform on binding";
          return false;
        }
        sawStatic = true;
      } else {
        if (visibilityTransform.has_value()) {
          error = "binding visibility transforms are mutually exclusive";
          return false;
        }
        visibilityTransform = transform.name;
      }
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
  if (restrictType.has_value()) {
    if (auto softwareType = findSoftwareNumericType(*restrictType)) {
      error = "software numeric types are not supported yet: " + *softwareType;
      return false;
    }
  }
  restrictTypeOut = restrictType;
  std::string fullType = typeName;
  if (typeHasTemplate) {
    fullType += "<" + info.typeTemplateArg + ">";
  }
  if (auto softwareType = findSoftwareNumericType(fullType)) {
    error = "software numeric types are not supported yet: " + *softwareType;
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

} // namespace primec::semantics
