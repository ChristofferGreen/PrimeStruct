#include "SemanticsValidator.h"

#include <cstdio>

namespace primec::semantics {

bool SemanticsValidator::validateExprFieldAccess(const std::vector<ParameterInfo> &params,
                                                 const std::unordered_map<std::string, BindingInfo> &locals,
                                                 const Expr &expr) {
  if (expr.name == "values" &&
      currentValidationState_.context.definitionPath == "/main") {
    const char *receiverKind = "other";
    std::string receiverName;
    if (!expr.args.empty()) {
      const Expr &receiverExpr = expr.args.front();
      switch (receiverExpr.kind) {
        case Expr::Kind::Name:
          receiverKind = "name";
          receiverName = receiverExpr.name;
          break;
        case Expr::Kind::Call:
          receiverKind = receiverExpr.isFieldAccess ? "field-call" : "call";
          receiverName = receiverExpr.name;
          break;
        case Expr::Kind::Literal:
          receiverKind = "literal";
          break;
        case Expr::Kind::BoolLiteral:
          receiverKind = "bool";
          break;
        case Expr::Kind::FloatLiteral:
          receiverKind = "float";
          break;
        case Expr::Kind::StringLiteral:
          receiverKind = "string";
          break;
      }
    }
    std::fprintf(stderr,
                 "DBG field-access line=%d name=%s def=%s params=%zu locals=%zu recv_kind=%s recv_name=%s\n",
                 expr.sourceLine,
                 expr.name.c_str(),
                 currentValidationState_.context.definitionPath.c_str(),
                 params.size(),
                 locals.size(),
                 receiverKind,
                 receiverName.c_str());
  }
  auto failFieldAccessDiagnostic = [&](std::string message) -> bool {
    return failExprDiagnostic(expr, std::move(message));
  };
  if (expr.args.size() != 1) {
    return failFieldAccessDiagnostic("field access requires a receiver");
  }
  if (!expr.templateArgs.empty()) {
    return failFieldAccessDiagnostic("field access does not accept template arguments");
  }
  if (hasNamedArguments(expr.argNames)) {
    return failFieldAccessDiagnostic("field access does not accept named arguments");
  }
  if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
    return failFieldAccessDiagnostic("field access does not accept block arguments");
  }
  std::string typeReceiverPath;
  const bool typeNamespaceReceiver =
      isTypeNamespaceFieldReceiver(params, locals, expr.args.front(), typeReceiverPath);
  if (!typeNamespaceReceiver && !validateExpr(params, locals, expr.args.front())) {
    return false;
  }
  BindingInfo fieldBinding;
  if (!resolveStructFieldBinding(params, locals, expr.args.front(), expr.name, fieldBinding)) {
    if (error_.empty()) {
      std::string receiverStructPath;
      if (!resolveStructFieldReceiverPath(params, locals, expr.args.front(), receiverStructPath)) {
        return failFieldAccessDiagnostic("field access requires struct receiver");
      } else {
        return failFieldAccessDiagnostic("unknown field: " + expr.name);
      }
    }
    return failExprDiagnostic(expr, error_);
  }
  error_.clear();
  return true;
}

bool SemanticsValidator::resolveStructFieldReceiverPath(const std::vector<ParameterInfo> &params,
                                                        const std::unordered_map<std::string, BindingInfo> &locals,
                                                        const Expr &receiverExpr,
                                                        std::string &structPathOut) {
  const std::string currentDefinitionNamespace = [&]() -> std::string {
    auto currentDefIt = defMap_.find(currentValidationState_.context.definitionPath);
    if (currentDefIt != defMap_.end() && currentDefIt->second != nullptr) {
      return currentDefIt->second->namespacePrefix;
    }
    const size_t slash = currentValidationState_.context.definitionPath.find_last_of('/');
    if (slash == std::string::npos || slash == 0) {
      return std::string{};
    }
    return currentValidationState_.context.definitionPath.substr(0, slash);
  }();
  auto resolveFieldBindingTarget = [&](const Expr &target, BindingInfo &bindingOut) -> bool {
    if (!(target.kind == Expr::Kind::Call && target.isFieldAccess && target.args.size() == 1)) {
      return false;
    }
    return resolveStructFieldBinding(params, locals, target.args.front(), target.name, bindingOut);
  };
  auto resolveStructPathFromType = [&](const std::string &typeName,
                                       const std::string &namespacePrefix,
                                       std::string &resolvedStructPathOut) -> bool {
      std::string normalizedTypeName = normalizeBindingTypeName(typeName);
      if (normalizedTypeName.empty() || isPrimitiveBindingTypeName(normalizedTypeName)) {
        return false;
      }
      std::string lookupTypeName = normalizedTypeName;
      std::string baseTypeName;
      std::string templateArgText;
      if (splitTemplateTypeName(normalizedTypeName, baseTypeName, templateArgText) &&
          !baseTypeName.empty()) {
        lookupTypeName = normalizeBindingTypeName(baseTypeName);
      }
      if (!lookupTypeName.empty() && lookupTypeName[0] == '/') {
        if (structNames_.count(lookupTypeName) > 0) {
          resolvedStructPathOut = lookupTypeName;
          return true;
        }
        return false;
      }
      std::string current = namespacePrefix;
      while (true) {
        if (!current.empty()) {
          std::string scoped = current + "/" + lookupTypeName;
          if (structNames_.count(scoped) > 0) {
            resolvedStructPathOut = scoped;
            return true;
          }
          if (current.size() > lookupTypeName.size()) {
            const size_t start = current.size() - lookupTypeName.size();
            if (start > 0 && current[start - 1] == '/' &&
                current.compare(start, lookupTypeName.size(), lookupTypeName) == 0 &&
                structNames_.count(current) > 0) {
              resolvedStructPathOut = current;
              return true;
            }
          }
        } else {
          std::string root = "/" + lookupTypeName;
          if (structNames_.count(root) > 0) {
            resolvedStructPathOut = root;
            return true;
          }
        }
        if (current.empty()) {
          break;
        }
        const size_t slash = current.find_last_of('/');
        if (slash == std::string::npos || slash == 0) {
          current.clear();
        } else {
          current.erase(slash);
        }
      }
      auto importIt = importAliases_.find(lookupTypeName);
      if (importIt != importAliases_.end() && structNames_.count(importIt->second) > 0) {
        resolvedStructPathOut = importIt->second;
        return true;
      }
      return false;
  };
  auto resolveArgsPackAccessTarget = [&](const Expr &target, std::string &elemType) -> bool {
    return resolveArgsPackElementTypeForExpr(target, params, locals, elemType);
  };
  auto resolveArrayTarget = [&](const Expr &target, std::string &elemType) -> bool {
    if (target.kind == Expr::Kind::Name) {
      auto resolveReference = [&](const BindingInfo &binding) -> bool {
        if (binding.typeName != "Reference" || binding.typeTemplateArg.empty()) {
          return false;
        }
        std::string base;
        std::string arg;
        if (!splitTemplateTypeName(binding.typeTemplateArg, base, arg) || base != "array") {
          return false;
        }
        std::vector<std::string> args;
        if (!splitTopLevelTemplateArgs(arg, args) || args.size() != 1) {
          return false;
        }
        elemType = args.front();
        return true;
      };
      if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
        if (resolveReference(*paramBinding)) {
          return true;
        }
        if ((paramBinding->typeName == "array" || paramBinding->typeName == "vector") &&
            !paramBinding->typeTemplateArg.empty()) {
          elemType = paramBinding->typeTemplateArg;
          return true;
        }
        return false;
      }
      auto it = locals.find(target.name);
      if (it == locals.end()) {
        return false;
      }
      if (resolveReference(it->second)) {
        return true;
      }
      if ((it->second.typeName == "array" || it->second.typeName == "vector") &&
          !it->second.typeTemplateArg.empty()) {
        elemType = it->second.typeTemplateArg;
        return true;
      }
      return false;
    }
    BindingInfo fieldBinding;
    if (resolveFieldBindingTarget(target, fieldBinding)) {
      std::string base;
      std::string arg;
      if (fieldBinding.typeName == "Reference" && !fieldBinding.typeTemplateArg.empty() &&
          splitTemplateTypeName(fieldBinding.typeTemplateArg, base, arg) && base == "array") {
        std::vector<std::string> args;
        if (splitTopLevelTemplateArgs(arg, args) && args.size() == 1) {
          elemType = args.front();
          return true;
        }
      }
      if ((fieldBinding.typeName == "array" || fieldBinding.typeName == "vector") &&
          !fieldBinding.typeTemplateArg.empty()) {
        elemType = fieldBinding.typeTemplateArg;
        return true;
      }
    }
    if (target.kind != Expr::Kind::Call) {
      return false;
    }
    std::string collectionTypePath;
    if (!resolveCallCollectionTypePath(target, params, locals, collectionTypePath) ||
        (collectionTypePath != "/array" && collectionTypePath != "/vector")) {
      return false;
    }
    std::vector<std::string> args;
    const std::string expectedBase = collectionTypePath == "/vector" ? "vector" : "array";
    if (resolveCallCollectionTemplateArgs(target, expectedBase, params, locals, args) &&
        args.size() == 1) {
      elemType = args.front();
    }
    return true;
  };
  auto resolveVectorTarget = [&](const Expr &target, std::string &elemType) -> bool {
    auto resolveVectorBinding = [&](const BindingInfo &binding) -> bool {
      if (binding.typeName == "vector" && !binding.typeTemplateArg.empty()) {
        elemType = binding.typeTemplateArg;
        return true;
      }
      const std::string normalizedType = normalizeBindingTypeName(binding.typeName);
      if (normalizedType == "Reference" || normalizedType == "Pointer") {
        return false;
      }
      return extractExperimentalVectorElementType(binding, elemType);
    };
    if (target.kind == Expr::Kind::Name) {
      if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
        return resolveVectorBinding(*paramBinding);
      }
      auto it = locals.find(target.name);
      return it != locals.end() && resolveVectorBinding(it->second);
    }
    BindingInfo fieldBinding;
    if (resolveFieldBindingTarget(target, fieldBinding) && resolveVectorBinding(fieldBinding)) {
      return true;
    }
    if (target.kind != Expr::Kind::Call) {
      return false;
    }
    std::string collectionTypePath;
    if (!resolveCallCollectionTypePath(target, params, locals, collectionTypePath) ||
        collectionTypePath != "/vector") {
      return false;
    }
    std::vector<std::string> args;
    if (resolveCallCollectionTemplateArgs(target, "vector", params, locals, args) &&
        args.size() == 1) {
      elemType = args.front();
    }
    return true;
  };

  structPathOut.clear();
  if (receiverExpr.kind == Expr::Kind::Name) {
    const BindingInfo *binding = findParamBinding(params, receiverExpr.name);
    if (!binding) {
      auto it = locals.find(receiverExpr.name);
      if (it != locals.end()) {
        binding = &it->second;
      }
    }
    const std::string receiverNamespace =
        !receiverExpr.namespacePrefix.empty() ? receiverExpr.namespacePrefix : currentDefinitionNamespace;
    if (binding) {
      std::string typeName = binding->typeName;
      if ((typeName == "Reference" || typeName == "Pointer") && !binding->typeTemplateArg.empty()) {
        typeName = binding->typeTemplateArg;
      }
      (void)resolveStructPathFromType(typeName, receiverNamespace, structPathOut);
    } else {
      (void)resolveStructPathFromType(receiverExpr.name, receiverNamespace, structPathOut);
    }
  } else if (receiverExpr.kind == Expr::Kind::Call && !receiverExpr.isBinding) {
    const std::string receiverNamespace =
        !receiverExpr.namespacePrefix.empty() ? receiverExpr.namespacePrefix : currentDefinitionNamespace;
    std::string accessName;
    if (getBuiltinArrayAccessName(receiverExpr, accessName) && receiverExpr.args.size() == 2) {
      std::string elemType;
      if (resolveArgsPackAccessTarget(receiverExpr.args.front(), elemType) ||
          resolveArrayTarget(receiverExpr.args.front(), elemType) ||
          resolveVectorTarget(receiverExpr.args.front(), elemType)) {
        const std::string unwrappedElemType = unwrapReferencePointerTypeText(elemType);
        (void)resolveStructPathFromType(unwrappedElemType, receiverNamespace, structPathOut);
      }
    }
    std::string inferredStruct = inferStructReturnPath(receiverExpr, params, locals);
    if (!inferredStruct.empty() && structNames_.count(inferredStruct) > 0) {
      structPathOut = inferredStruct;
    } else {
      std::string inferredTypeText;
      if (inferQueryExprTypeText(receiverExpr, params, locals, inferredTypeText)) {
        const std::string inferredValueType =
            unwrapReferencePointerTypeText(normalizeBindingTypeName(inferredTypeText));
        (void)resolveStructPathFromType(inferredValueType, receiverNamespace, structPathOut);
      }
    }
    if (structPathOut.empty()) {
      std::string resolvedType = resolveCalleePath(receiverExpr);
      if (structNames_.count(resolvedType) > 0) {
        structPathOut = resolvedType;
      }
    }
  }
  return !structPathOut.empty();
}

bool SemanticsValidator::isTypeNamespaceFieldReceiver(const std::vector<ParameterInfo> &params,
                                                      const std::unordered_map<std::string, BindingInfo> &locals,
                                                      const Expr &receiverExpr,
                                                      std::string &structPathOut) {
  structPathOut.clear();
  if (receiverExpr.kind != Expr::Kind::Name) {
    return false;
  }
  if (findParamBinding(params, receiverExpr.name) != nullptr || locals.find(receiverExpr.name) != locals.end()) {
    return false;
  }
  return resolveStructFieldReceiverPath(params, locals, receiverExpr, structPathOut);
}

bool SemanticsValidator::resolveStructFieldBinding(const std::vector<ParameterInfo> &params,
                                                   const std::unordered_map<std::string, BindingInfo> &locals,
                                                   const Expr &receiver,
                                                   const std::string &fieldName,
                                                   BindingInfo &bindingOut) {
  auto failFieldResolutionDiagnostic = [&](const Expr &diagnosticExpr,
                                           std::string message) -> bool {
    return failExprDiagnostic(diagnosticExpr, std::move(message));
  };
  auto bindingTypeText = [](const BindingInfo &binding) -> std::string {
    if (binding.typeTemplateArg.empty()) {
      return binding.typeName;
    }
    return binding.typeName + "<" + binding.typeTemplateArg + ">";
  };
  auto assignBindingTypeFromText = [](const std::string &typeText, BindingInfo &bindingOut) -> bool {
    const std::string normalizedType = normalizeBindingTypeName(typeText);
    if (normalizedType.empty()) {
      return false;
    }
    std::string base;
    std::string argText;
    if (splitTemplateTypeName(normalizedType, base, argText)) {
      bindingOut.typeName = normalizeBindingTypeName(base);
      bindingOut.typeTemplateArg = argText;
      return true;
    }
    bindingOut.typeName = normalizedType;
    bindingOut.typeTemplateArg.clear();
    return true;
  };
  auto inferStructFieldBinding = [&](const Definition &def,
                                     const std::string &bindingFieldName,
                                     BindingInfo &fieldBindingOut,
                                     bool allowStatic,
                                     bool allowPrivate) -> bool {
    auto isStaticField = [](const Expr &stmt) {
      for (const auto &transform : stmt.transforms) {
        if (transform.name == "static") {
          return true;
        }
      }
      return false;
    };
    auto isPrivateField = [](const Expr &stmt) {
      for (const auto &transform : stmt.transforms) {
        if (transform.name == "private") {
          return true;
        }
      }
      return false;
    };
    for (const auto &stmt : def.statements) {
      if (!stmt.isBinding) {
        return failFieldResolutionDiagnostic(
            receiver, "struct definitions may only contain field bindings: " + def.fullPath);
      }
      if (stmt.name != bindingFieldName) {
        continue;
      }
      if (isStaticField(stmt) && !allowStatic) {
        return false;
      }
      if (isPrivateField(stmt) && !allowPrivate) {
        return failFieldResolutionDiagnostic(
            receiver, "private field is not accessible: " + def.fullPath + "/" + bindingFieldName);
      }
      BindingInfo fieldBinding;
      if (!SemanticsValidator::resolveStructFieldBinding(def, stmt, fieldBinding)) {
        return false;
      }
      fieldBindingOut = std::move(fieldBinding);
      return true;
    }
    return false;
  };
  const std::string currentDefinitionNamespace = [&]() -> std::string {
    auto currentDefIt = defMap_.find(currentValidationState_.context.definitionPath);
    if (currentDefIt != defMap_.end() && currentDefIt->second != nullptr) {
      return currentDefIt->second->namespacePrefix;
    }
    const size_t slash = currentValidationState_.context.definitionPath.find_last_of('/');
    if (slash == std::string::npos || slash == 0) {
      return std::string{};
    }
    return currentValidationState_.context.definitionPath.substr(0, slash);
  }();

  std::string structPath;
  const bool isTypeReceiver = isTypeNamespaceFieldReceiver(params, locals, receiver, structPath);
  if (!isTypeReceiver && !resolveStructFieldReceiverPath(params, locals, receiver, structPath)) {
    if (fieldName == "values" && currentValidationState_.context.definitionPath == "/main") {
      std::fprintf(stderr,
                   "DBG resolveStructFieldBinding failed receiver-path field=%s recv_kind=%d recv_name=%s locals=%zu\n",
                   fieldName.c_str(),
                   static_cast<int>(receiver.kind),
                   receiver.name.c_str(),
                   locals.size());
    }
    return false;
  }
  if (fieldName == "values" && currentValidationState_.context.definitionPath == "/main") {
    std::fprintf(stderr,
                 "DBG resolveStructFieldBinding field=%s structPath=%s isType=%d locals=%zu\n",
                 fieldName.c_str(),
                 structPath.c_str(),
                 isTypeReceiver ? 1 : 0,
                 locals.size());
  }
  auto defIt = defMap_.find(structPath);
  if (defIt == defMap_.end() || !defIt->second) {
    if (fieldName == "values" && currentValidationState_.context.definitionPath == "/main") {
      std::fprintf(stderr,
                   "DBG resolveStructFieldBinding missing def structPath=%s\n",
                   structPath.c_str());
    }
    return false;
  }
  const Definition *fieldSourceDef = defIt->second;
  const Definition *templateSourceDef = fieldSourceDef;
  if (templateSourceDef->templateArgs.empty()) {
    const size_t lastSlash = structPath.find_last_of('/');
    const size_t nameStart = lastSlash == std::string::npos ? 0 : lastSlash + 1;
    const size_t suffix = structPath.find("__t", nameStart);
    if (suffix != std::string::npos) {
      const std::string basePath = structPath.substr(0, suffix);
      auto baseIt = defMap_.find(basePath);
      if (baseIt != defMap_.end() && baseIt->second != nullptr && !baseIt->second->templateArgs.empty()) {
        templateSourceDef = baseIt->second;
      }
    }
  }
  if (isTypeReceiver) {
    auto isStaticField = [](const Expr &stmt) {
      for (const auto &transform : stmt.transforms) {
        if (transform.name == "static") {
          return true;
        }
      }
      return false;
    };
    for (const auto &stmt : defIt->second->statements) {
      if (!stmt.isBinding || !isStaticField(stmt) || stmt.name != fieldName) {
        continue;
      }
      return SemanticsValidator::resolveStructFieldBinding(*defIt->second, stmt, bindingOut);
    }
    return false;
  }
  BindingInfo inferred;
  if (!inferStructFieldBinding(*templateSourceDef, fieldName, inferred, false, true)) {
    if (fieldName == "values" && currentValidationState_.context.definitionPath == "/main") {
      std::fprintf(stderr,
                   "DBG resolveStructFieldBinding infer field failed templateDef=%s fieldSource=%s\n",
                   templateSourceDef->fullPath.c_str(),
                   fieldSourceDef->fullPath.c_str());
    }
    return false;
  }
  BindingInfo receiverBinding;
  if (inferBindingTypeFromInitializer(receiver, params, locals, receiverBinding) &&
      !receiverBinding.typeTemplateArg.empty() &&
      !templateSourceDef->templateArgs.empty()) {
    std::string receiverTypeText = bindingTypeText(receiverBinding);
    std::string receiverBase;
    std::string receiverArgText;
    if (splitTemplateTypeName(normalizeBindingTypeName(receiverTypeText), receiverBase, receiverArgText)) {
      const std::string receiverNamespace =
          !receiver.namespacePrefix.empty() ? receiver.namespacePrefix : currentDefinitionNamespace;
      std::string resolvedReceiverBase = resolveStructTypePath(receiverBase, receiverNamespace, structNames_);
      if (resolvedReceiverBase.empty()) {
        resolvedReceiverBase = resolveTypePath(receiverBase, receiverNamespace);
      }
      std::vector<std::string> receiverArgs;
      if ((resolvedReceiverBase == templateSourceDef->fullPath || resolvedReceiverBase == fieldSourceDef->fullPath ||
           resolvedReceiverBase == structPath) &&
          splitTopLevelTemplateArgs(receiverArgText, receiverArgs) &&
          receiverArgs.size() == templateSourceDef->templateArgs.size()) {
        std::unordered_map<std::string, std::string> templateArgMap;
        for (size_t i = 0; i < receiverArgs.size(); ++i) {
          templateArgMap.emplace(normalizeBindingTypeName(templateSourceDef->templateArgs[i]),
                                 normalizeBindingTypeName(receiverArgs[i]));
        }
        std::function<std::string(const std::string &)> substituteTypeText =
            [&](const std::string &typeText) -> std::string {
          const std::string normalizedType = normalizeBindingTypeName(typeText);
          if (normalizedType.empty()) {
            return normalizedType;
          }
          auto directIt = templateArgMap.find(normalizedType);
          if (directIt != templateArgMap.end()) {
            return directIt->second;
          }
          std::string base;
          std::string argText;
          if (!splitTemplateTypeName(normalizedType, base, argText)) {
            return normalizedType;
          }
          std::vector<std::string> args;
          if (!splitTopLevelTemplateArgs(argText, args)) {
            return normalizedType;
          }
          std::string substitutedBase = substituteTypeText(base);
          for (std::string &arg : args) {
            arg = substituteTypeText(arg);
          }
          return substitutedBase + "<" + joinTemplateArgs(args) + ">";
        };
        BindingInfo specializedFieldBinding = inferred;
        const std::string specializedFieldTypeText =
            substituteTypeText(bindingTypeText(inferred));
        if (assignBindingTypeFromText(specializedFieldTypeText, specializedFieldBinding)) {
          inferred = std::move(specializedFieldBinding);
        }
      }
    }
  }
  bindingOut = std::move(inferred);
  return true;
}

bool SemanticsValidator::isTypeNamespaceMethodCall(const std::vector<ParameterInfo> &params,
                                                   const std::unordered_map<std::string, BindingInfo> &locals,
                                                   const Expr &callExpr,
                                                   const std::string &resolvedPath) const {
  if (!callExpr.isMethodCall || callExpr.args.empty() || resolvedPath.empty()) {
    return false;
  }
  const Expr &receiver = callExpr.args.front();
  if (receiver.kind != Expr::Kind::Name) {
    return false;
  }
  if (findParamBinding(params, receiver.name) != nullptr || locals.find(receiver.name) != locals.end()) {
    return false;
  }
  const size_t methodSlash = resolvedPath.find_last_of('/');
  if (methodSlash == std::string::npos || methodSlash == 0) {
    return false;
  }
  const std::string receiverPath = resolvedPath.substr(0, methodSlash);
  const size_t receiverSlash = receiverPath.find_last_of('/');
  const std::string receiverTypeName =
      receiverSlash == std::string::npos ? receiverPath : receiverPath.substr(receiverSlash + 1);
  return receiverTypeName == receiver.name;
}

std::string SemanticsValidator::describeMethodReflectionTarget(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &callExpr) const {
  auto isUnboundMetaReceiver = [&](const Expr &receiver) {
    if (receiver.kind != Expr::Kind::Name || receiver.name != "meta") {
      return false;
    }
    if (findParamBinding(params, receiver.name) != nullptr) {
      return false;
    }
    return locals.find(receiver.name) == locals.end();
  };

  if (!callExpr.isMethodCall || callExpr.args.empty()) {
    return "";
  }
  const Expr &receiver = callExpr.args.front();
  if (!isUnboundMetaReceiver(receiver)) {
    return "";
  }
  return "meta." + callExpr.name;
}

} // namespace primec::semantics
