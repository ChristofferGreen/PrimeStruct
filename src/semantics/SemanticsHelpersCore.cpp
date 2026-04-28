#include "SemanticsHelpers.h"

#include <cctype>
#include <functional>
#include <limits>

namespace primec::semantics {

bool validateAlignTransform(const Transform &transform, const std::string &context, std::string &error);

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
  if (getBuiltinMemoryName(expr, builtinName)) {
    if (builtinName == "alloc") {
      return expr.templateArgs.size() == 1 && expr.args.size() == 1;
    }
    if (builtinName == "realloc" && expr.args.size() == 2) {
      return isPointerExpr(expr.args.front(), params, locals);
    }
    if (builtinName == "at" && expr.templateArgs.empty() && expr.args.size() == 3) {
      return isPointerExpr(expr.args.front(), params, locals);
    }
    if (builtinName == "at_unsafe" && expr.templateArgs.empty() && expr.args.size() == 2) {
      return isPointerExpr(expr.args.front(), params, locals);
    }
    if (builtinName == "reinterpret" && expr.templateArgs.size() == 1 && expr.args.size() == 1) {
      return isPointerExpr(expr.args.front(), params, locals);
    }
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
  std::string accessName;
  if (getBuiltinArrayAccessName(expr, accessName) && expr.args.size() == 2) {
    size_t receiverIndex = 0;
    if (!expr.isMethodCall) {
      bool foundValues = false;
      for (size_t i = 0; i < expr.args.size(); ++i) {
        if (i < expr.argNames.size() && expr.argNames[i].has_value() && *expr.argNames[i] == "values") {
          receiverIndex = i;
          foundValues = true;
          break;
        }
      }
      if (!foundValues) {
        receiverIndex = 0;
      }
    }
    if (receiverIndex < expr.args.size() && expr.args[receiverIndex].kind == Expr::Kind::Name) {
      const BindingInfo *binding = findBinding(params, locals, expr.args[receiverIndex].name);
      std::string elementType;
      if (binding != nullptr && getArgsPackElementType(*binding, elementType)) {
        std::string base;
        std::string argText;
        if (splitTemplateTypeName(normalizeBindingTypeName(elementType), base, argText)) {
          base = normalizeBindingTypeName(base);
          if (base == "Pointer" || base == "Reference") {
            return true;
          }
        }
      }
    }
  }
  std::string builtinName;
  if (getBuiltinPointerName(expr, builtinName) && builtinName == "location" && expr.args.size() == 1) {
    return true;
  }
  if (getBuiltinMemoryName(expr, builtinName)) {
    if (builtinName == "alloc") {
      return expr.templateArgs.size() == 1 && expr.args.size() == 1;
    }
    if (builtinName == "realloc" && expr.args.size() == 2) {
      return isPointerExpr(expr.args.front(), params, locals);
    }
    if (builtinName == "at" && expr.templateArgs.empty() && expr.args.size() == 3) {
      return isPointerExpr(expr.args.front(), params, locals);
    }
    if (builtinName == "at_unsafe" && expr.templateArgs.empty() && expr.args.size() == 2) {
      return isPointerExpr(expr.args.front(), params, locals);
    }
  }
  if (getBuiltinOperatorName(expr, builtinName) && (builtinName == "plus" || builtinName == "minus") &&
      expr.args.size() == 2) {
    return isPointerLikeExpr(expr.args[0], params, locals) && !isPointerLikeExpr(expr.args[1], params, locals);
  }
  return false;
}

bool parseBindingInfo(const Expr &expr,
                      const std::string &namespacePrefix,
                      const std::unordered_set<std::string> &structTypes,
                      const std::unordered_map<std::string, std::string> &importAliases,
                      BindingInfo &info,
                      std::optional<std::string> &restrictTypeOut,
                      std::string &error,
                      const std::unordered_set<std::string> *additionalNominalTypes) {
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
    if (transform.name == "stack" || transform.name == "heap" || transform.name == "buffer") {
      error = "placement transforms are not supported on bindings";
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
    if (transform.name == "pod" || transform.name == "gpu_lane") {
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
    if (transform.name == "handle") {
      if (transform.templateArgs.size() > 1) {
        error = "handle accepts at most one template argument";
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
      const std::string normalizedTypeName = normalizeBindingTypeName(transform.name);
      if ((normalizedTypeName == "array" || normalizedTypeName == "vector" || normalizedTypeName == "Buffer") &&
          transform.templateArgs.size() != 1) {
        if (normalizedTypeName == "array" && transform.templateArgs.size() > 1) {
          error = "array<T, N> is unsupported; use array<T> (runtime-count array)";
          return false;
        }
        error = normalizedTypeName + " requires exactly one template argument";
        return false;
      }
      if (normalizedTypeName == "map" && transform.templateArgs.size() != 2) {
        error = "map requires exactly two template arguments";
        return false;
      }
      if (transform.name == "soa_vector") {
        if (transform.templateArgs.size() != 1) {
          error = "soa_vector requires exactly one template argument";
          return false;
        }
        if (!isSoaVectorStructElementType(transform.templateArgs.front(), namespacePrefix, structTypes, importAliases)) {
          error = "soa_vector requires struct element type";
          return false;
        }
        typeName = transform.name;
        typeHasTemplate = true;
        info.typeTemplateArg = joinTemplateArgs(transform.templateArgs);
        continue;
      }
      if (transform.name == "uninitialized" && transform.templateArgs.size() != 1) {
        error = "uninitialized requires exactly one template argument";
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
  if (typeName == "Self") {
    if (structTypes.count(namespacePrefix) == 0) {
      error = "Self is only valid inside struct definitions";
      return false;
    }
    typeName = namespacePrefix;
  }
  if (typeHasTemplate && info.typeTemplateArg == "Self") {
    if (structTypes.count(namespacePrefix) == 0) {
      error = "Self is only valid inside struct definitions";
      return false;
    }
    info.typeTemplateArg = namespacePrefix;
  }
  restrictTypeOut = restrictType;
  std::string fullType = typeName;
  if (typeHasTemplate) {
    fullType += "<" + info.typeTemplateArg + ">";
  }
  auto resolveAdditionalNominalTypePath = [&](const std::string &candidate) -> std::string {
    if (additionalNominalTypes == nullptr || candidate.empty()) {
      return {};
    }
    if (candidate.front() == '/') {
      return additionalNominalTypes->count(candidate) > 0 ? candidate : std::string{};
    }
    if (!namespacePrefix.empty()) {
      const size_t lastSlash = namespacePrefix.find_last_of('/');
      const std::string_view suffix = lastSlash == std::string::npos
                                          ? std::string_view(namespacePrefix)
                                          : std::string_view(namespacePrefix).substr(lastSlash + 1);
      if (suffix == candidate && additionalNominalTypes->count(namespacePrefix) > 0) {
        return namespacePrefix;
      }
      std::string prefix = namespacePrefix;
      while (!prefix.empty()) {
        const std::string scoped = prefix + "/" + candidate;
        if (additionalNominalTypes->count(scoped) > 0) {
          return scoped;
        }
        const size_t slash = prefix.find_last_of('/');
        if (slash == std::string::npos) {
          break;
        }
        prefix = prefix.substr(0, slash);
      }
    }
    const std::string root = "/" + candidate;
    if (additionalNominalTypes->count(root) > 0) {
      return root;
    }
    auto importIt = importAliases.find(candidate);
    if (importIt != importAliases.end() &&
        additionalNominalTypes->count(importIt->second) > 0) {
      return importIt->second;
    }
    return {};
  };
  if (!isPrimitiveBindingTypeName(typeName) && !typeHasTemplate) {
    std::string resolved = resolveStructTypePath(typeName, namespacePrefix, structTypes);
    if (resolved.empty()) {
      auto importIt = importAliases.find(typeName);
      if (importIt != importAliases.end() && structTypes.count(importIt->second) > 0) {
        resolved = importIt->second;
      }
    }
    if (resolved.empty()) {
      resolved = resolveAdditionalNominalTypePath(typeName);
    }
    if (resolved.empty()) {
      if (typeName != "FileError") {
        error = "unsupported binding type: " + typeName;
        return false;
      }
    }
  }
  auto isStructTypeName = [&](const std::string &candidate) -> bool {
    if (candidate.empty() || candidate.find('<') != std::string::npos) {
      return false;
    }
    std::string resolved = resolveStructTypePath(candidate, namespacePrefix, structTypes);
    if (!resolved.empty()) {
      return true;
    }
    auto importIt = importAliases.find(candidate);
    if (importIt != importAliases.end()) {
      return structTypes.count(importIt->second) > 0;
    }
    return !resolveAdditionalNominalTypePath(candidate).empty();
  };
  auto trimTypeText = [](const std::string &value) -> std::string {
    size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])) != 0) {
      ++start;
    }
    size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
      --end;
    }
    return value.substr(start, end - start);
  };
  auto containsUninitializedType = [&](const std::string &text) -> bool {
    std::function<bool(const std::string &)> contains;
    contains = [&](const std::string &value) -> bool {
      std::string trimmed = trimTypeText(value);
      if (trimmed.empty()) {
        return false;
      }
      if (!trimmed.empty() && trimmed[0] == '/') {
        trimmed.erase(0, 1);
      }
      std::string base;
      std::string arg;
      if (splitTemplateTypeName(trimmed, base, arg)) {
        base = trimTypeText(base);
        if (!base.empty() && base[0] == '/') {
          base.erase(0, 1);
        }
        if (base == "uninitialized") {
          return true;
        }
        std::vector<std::string> args;
        if (splitTopLevelTemplateArgs(arg, args)) {
          for (const auto &entry : args) {
            if (contains(entry)) {
              return true;
            }
          }
        }
        return false;
      }
      return trimmed == "uninitialized";
    };
    return contains(text);
  };
  auto extractTopLevelUninitializedTarget = [&](const std::string &typeText, std::string &innerTypeOut) -> bool {
    std::string base;
    std::string argText;
    if (!splitTemplateTypeName(trimTypeText(typeText), base, argText)) {
      return false;
    }
    if (normalizeBindingTypeName(trimTypeText(base)) != "uninitialized") {
      return false;
    }
    innerTypeOut = trimTypeText(argText);
    return !innerTypeOut.empty();
  };
  auto isSupportedPointerReferenceTarget = [&](const std::string &targetType, std::string &unsupportedError) -> bool {
    const std::string normalizedTarget = normalizeBindingTypeName(targetType);
    if (isPrimitiveBindingTypeName(normalizedTarget) || isStructTypeName(normalizedTarget) ||
        normalizedTarget == "FileError") {
      return true;
    }

    std::string base;
    std::string arg;
    if (!splitTemplateTypeName(targetType, base, arg)) {
      unsupportedError = targetType;
      return false;
    }

    base = normalizeBindingTypeName(base);
    std::vector<std::string> args;
    if (!splitTopLevelTemplateArgs(arg, args)) {
      unsupportedError = targetType;
      return false;
    }

    if ((base == "array" || base == "vector" || base == "soa_vector" || base == "Buffer" || base == "File") &&
        args.size() == 1) {
      return true;
    }
    if (base == "map" && args.size() == 2) {
      return true;
    }
    if (base == "Result" && (args.size() == 1 || args.size() == 2)) {
      return true;
    }

    unsupportedError = targetType;
    return false;
  };
  auto isSupportedArgsUninitializedElementType = [&](const std::string &elementType, std::string &elementError) -> bool {
    elementError.clear();
    std::string elementBase;
    std::string elementArgText;
    if (!splitTemplateTypeName(trimTypeText(elementType), elementBase, elementArgText)) {
      return false;
    }
    elementBase = normalizeBindingTypeName(trimTypeText(elementBase));
    const bool isPointerElement = elementBase == "Pointer";
    const bool isReferenceElement = elementBase == "Reference";
    if (!isPointerElement && !isReferenceElement) {
      return false;
    }
    std::vector<std::string> elementArgs;
    if (!splitTopLevelTemplateArgs(elementArgText, elementArgs) || elementArgs.size() != 1) {
      return false;
    }

    std::string targetType = elementArgs.front();
    if (extractTopLevelUninitializedTarget(targetType, targetType)) {
      if (containsUninitializedType(targetType)) {
        elementError = isPointerElement ? "uninitialized storage is not allowed in pointer targets"
                                        : "uninitialized storage is not allowed in reference targets";
        return false;
      }
    } else if (containsUninitializedType(elementArgs.front())) {
      elementError = "uninitialized storage is not allowed as template argument to user-defined types";
      return false;
    }

    std::string unsupportedTarget;
    if (!isSupportedPointerReferenceTarget(targetType, unsupportedTarget)) {
      elementError = isPointerElement ? "unsupported pointer target type: " + unsupportedTarget
                                      : "unsupported reference target type: " + unsupportedTarget;
      return false;
    }
    return true;
  };
  const std::string normalizedTypeName = normalizeBindingTypeName(typeName);
  if (typeHasTemplate &&
      (normalizedTypeName == "array" || normalizedTypeName == "vector" || normalizedTypeName == "Buffer")) {
    if (containsUninitializedType(info.typeTemplateArg)) {
      error = "uninitialized storage is not allowed in " + normalizedTypeName + " element types";
      return false;
    }
  }
  if (typeHasTemplate && normalizedTypeName == "map") {
    std::vector<std::string> args;
    if (!splitTopLevelTemplateArgs(info.typeTemplateArg, args) || args.size() != 2) {
      error = "map requires exactly two template arguments";
      return false;
    }
    if (containsUninitializedType(args[0]) || containsUninitializedType(args[1])) {
      error = "uninitialized storage is not allowed in map key/value types";
      return false;
    }
  }
  if (typeHasTemplate && normalizedTypeName != "Pointer" && normalizedTypeName != "Reference" &&
      normalizedTypeName != "uninitialized" && normalizedTypeName != "array" &&
      normalizedTypeName != "vector" && normalizedTypeName != "map" && normalizedTypeName != "Buffer" &&
      normalizedTypeName != "Result" && normalizedTypeName != "args") {
    if (containsUninitializedType(info.typeTemplateArg)) {
      error = "uninitialized storage is not allowed as template argument to user-defined types";
      return false;
    }
  }
  if (typeHasTemplate && normalizedTypeName == "args" && containsUninitializedType(info.typeTemplateArg)) {
    std::string elementError;
    if (!isSupportedArgsUninitializedElementType(info.typeTemplateArg, elementError)) {
      error = elementError.empty() ? "uninitialized storage is not allowed as template argument to user-defined types"
                                   : elementError;
      return false;
    }
  }
  if (typeHasTemplate && typeName == "Pointer") {
    if (info.typeTemplateArg.empty()) {
      error = "Pointer requires a template argument";
      return false;
    }
    std::string pointerTargetType = info.typeTemplateArg;
    if (extractTopLevelUninitializedTarget(info.typeTemplateArg, pointerTargetType)) {
      if (containsUninitializedType(pointerTargetType)) {
        error = "uninitialized storage is not allowed in pointer targets";
        return false;
      }
    } else if (containsUninitializedType(info.typeTemplateArg)) {
      error = "uninitialized storage is not allowed in pointer targets";
      return false;
    }
    std::string unsupportedTarget;
    if (!isSupportedPointerReferenceTarget(pointerTargetType, unsupportedTarget)) {
      error = "unsupported pointer target type: " + unsupportedTarget;
      return false;
    }
  }
  if (typeHasTemplate && typeName == "Reference") {
    if (info.typeTemplateArg.empty()) {
      error = "Reference requires a template argument";
      return false;
    }
    std::string referenceTargetType = info.typeTemplateArg;
    if (extractTopLevelUninitializedTarget(info.typeTemplateArg, referenceTargetType)) {
      if (containsUninitializedType(referenceTargetType)) {
        error = "uninitialized storage is not allowed in reference targets";
        return false;
      }
    } else if (containsUninitializedType(info.typeTemplateArg)) {
      error = "uninitialized storage is not allowed in reference targets";
      return false;
    }
    std::string unsupportedTarget;
    if (!isSupportedPointerReferenceTarget(referenceTargetType, unsupportedTarget)) {
      error = "unsupported reference target type: " + unsupportedTarget;
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
