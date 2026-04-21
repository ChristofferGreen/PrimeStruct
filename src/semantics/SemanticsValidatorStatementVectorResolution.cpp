#include "SemanticsValidator.h"

#include <string>
#include <string_view>
#include <vector>

namespace primec::semantics {
namespace {

ReturnKind returnKindForStatementBinding(const BindingInfo &binding) {
  if (binding.typeName == "Reference") {
    std::string base;
    std::string arg;
    if (splitTemplateTypeName(binding.typeTemplateArg, base, arg) && base == "array") {
      std::vector<std::string> args;
      if (splitTopLevelTemplateArgs(arg, args) && args.size() == 1) {
        return ReturnKind::Array;
      }
    }
    return returnKindForTypeName(binding.typeTemplateArg);
  }
  return returnKindForTypeName(binding.typeName);
}

} // namespace

bool SemanticsValidator::isVectorStatementIntegerExpr(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &arg) {
  ReturnKind kind = inferExprReturnKind(arg, params, locals);
  if (kind == ReturnKind::Int || kind == ReturnKind::Int64 || kind == ReturnKind::UInt64) {
    return true;
  }
  if (kind == ReturnKind::Bool || kind == ReturnKind::Float32 || kind == ReturnKind::Float64 ||
      kind == ReturnKind::Void || kind == ReturnKind::Array) {
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
        ReturnKind paramKind = returnKindForStatementBinding(*paramBinding);
        return paramKind == ReturnKind::Int || paramKind == ReturnKind::Int64 ||
               paramKind == ReturnKind::UInt64;
      }
      auto it = locals.find(arg.name);
      if (it != locals.end()) {
        ReturnKind localKind = returnKindForStatementBinding(it->second);
        return localKind == ReturnKind::Int || localKind == ReturnKind::Int64 ||
               localKind == ReturnKind::UInt64;
      }
    }
    return true;
  }
  return false;
}

bool SemanticsValidator::resolveVectorStatementBinding(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &target,
    BindingInfo &bindingOut) {
  auto resolveNamedBinding = [&](const std::string &name) -> const BindingInfo * {
    if (const BindingInfo *paramBinding = findParamBinding(params, name)) {
      return paramBinding;
    }
    auto it = locals.find(name);
    if (it != locals.end()) {
      return &it->second;
    }
    return nullptr;
  };
  auto resolveVectorTypeText = [&](const std::string &typeText, bool isMutable) -> bool {
    if (typeText.empty()) {
      return false;
    }
    std::string normalizedType = normalizeBindingTypeName(typeText);
    std::string base;
    std::string arg;
    if (splitTemplateTypeName(normalizedType, base, arg)) {
      const std::string normalizedBase = normalizeBindingTypeName(base);
      if ((normalizedBase == "vector" || normalizedBase == "soa_vector") && !arg.empty()) {
        bindingOut = {};
        bindingOut.typeName = normalizedBase;
        bindingOut.typeTemplateArg = arg;
        bindingOut.isMutable = isMutable;
        return true;
      }
    } else if (normalizedType == "vector" || normalizedType == "soa_vector") {
      bindingOut = {};
      bindingOut.typeName = normalizedType;
      bindingOut.typeTemplateArg.clear();
      bindingOut.isMutable = isMutable;
      return true;
    }
    BindingInfo inferredBinding;
    inferredBinding.typeName = normalizedType;
    if (splitTemplateTypeName(normalizedType, base, arg)) {
      inferredBinding.typeName = normalizeBindingTypeName(base);
      inferredBinding.typeTemplateArg = arg;
    }
    std::string elemType;
    if (!extractExperimentalVectorElementType(inferredBinding, elemType)) {
      return false;
    }
    bindingOut = {};
    bindingOut.typeName = "Vector";
    bindingOut.typeTemplateArg = elemType;
    bindingOut.isMutable = isMutable;
    return true;
  };

  if (target.kind == Expr::Kind::Name) {
    if (const BindingInfo *binding = resolveNamedBinding(target.name)) {
      if ((binding->typeName == "vector" || binding->typeName == "soa_vector") &&
          !binding->typeTemplateArg.empty()) {
        bindingOut = *binding;
        return true;
      }
      std::string elemType;
      if (!extractExperimentalVectorElementType(*binding, elemType)) {
        return false;
      }
      bindingOut = {};
      bindingOut.typeName = "Vector";
      bindingOut.typeTemplateArg = elemType;
      bindingOut.isMutable = binding->isMutable;
      return true;
    }
    return false;
  }
  if (target.kind == Expr::Kind::Call) {
    std::string accessName;
    if (getBuiltinArrayAccessName(target, accessName) && target.args.size() == 2 &&
        target.args.front().kind == Expr::Kind::Name) {
      if (const BindingInfo *binding = resolveNamedBinding(target.args.front().name)) {
        std::string elementType;
        if (getArgsPackElementType(*binding, elementType)) {
          return resolveVectorTypeText(elementType, true);
        }
      }
      return false;
    }
    if (isSimpleCallName(target, "dereference") && target.args.size() == 1) {
      const Expr &derefTarget = target.args.front();
      if (derefTarget.kind == Expr::Kind::Name) {
        const BindingInfo *binding = resolveNamedBinding(derefTarget.name);
        if (binding == nullptr || binding->typeTemplateArg.empty()) {
          return false;
        }
        const std::string normalizedType = normalizeBindingTypeName(binding->typeName);
        if (normalizedType != "Reference" && normalizedType != "Pointer") {
          return false;
        }
        return resolveVectorTypeText(binding->typeTemplateArg, true);
      }
      if (getBuiltinArrayAccessName(derefTarget, accessName) && derefTarget.args.size() == 2 &&
          derefTarget.args.front().kind == Expr::Kind::Name) {
        if (const BindingInfo *binding = resolveNamedBinding(derefTarget.args.front().name)) {
          std::string elementType;
          if (getArgsPackElementType(*binding, elementType)) {
            std::string base;
            std::string arg;
            if (splitTemplateTypeName(normalizeBindingTypeName(elementType), base, arg)) {
              const std::string normalizedBase = normalizeBindingTypeName(base);
              if ((normalizedBase == "Reference" || normalizedBase == "Pointer") && !arg.empty()) {
                return resolveVectorTypeText(arg, true);
              }
            }
          }
        }
      }
      return false;
    }
  }
  return false;
}

bool SemanticsValidator::validateVectorStatementElementType(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &arg,
    const std::string &typeName) {
  auto failVectorElementDiagnostic = [&](std::string message) -> bool {
    return failExprDiagnostic(arg, std::move(message));
  };
  if (typeName.empty()) {
    return failVectorElementDiagnostic("push requires vector element type");
  }
  const std::string normalizedType = normalizeBindingTypeName(typeName);
  auto isStringValueExpr = [&](const Expr &candidate) {
    if (candidate.kind == Expr::Kind::StringLiteral) {
      return true;
    }
    if (candidate.kind == Expr::Kind::Name) {
      if (const BindingInfo *paramBinding = findParamBinding(params, candidate.name)) {
        return paramBinding->typeName == "string";
      }
      auto it = locals.find(candidate.name);
      return it != locals.end() && it->second.typeName == "string";
    }
    return inferExprReturnKind(candidate, params, locals) == ReturnKind::String;
  };
  if (normalizedType == "string") {
    if (!isStringValueExpr(arg)) {
      return failVectorElementDiagnostic("push requires element type " + typeName);
    }
    return true;
  }
  ReturnKind expectedKind = returnKindForTypeName(normalizedType);
  if (expectedKind == ReturnKind::Unknown) {
    return true;
  }
  if (isStringValueExpr(arg)) {
    return failVectorElementDiagnostic("push requires element type " + typeName);
  }
  ReturnKind argKind = inferExprReturnKind(arg, params, locals);
  if (argKind != ReturnKind::Unknown && argKind != expectedKind) {
    return failVectorElementDiagnostic("push requires element type " + typeName);
  }
  return true;
}

bool SemanticsValidator::validateVectorStatementHelperTarget(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &target,
    const char *helperName,
    BindingInfo &bindingOut) {
  auto failVectorTargetDiagnostic = [&](std::string message) -> bool {
    return failExprDiagnostic(target, std::move(message));
  };
  const std::string helper = helperName == nullptr ? "" : std::string(helperName);
  const bool allowSoaVectorTarget = helper == "push" || helper == "reserve";
  if (!resolveVectorStatementBinding(params, locals, target, bindingOut)) {
    return failVectorTargetDiagnostic(std::string(helperName) + " requires vector binding");
  }
  std::string experimentalElemType;
  const bool isExperimentalVectorTarget = extractExperimentalVectorElementType(bindingOut, experimentalElemType);
  if (bindingOut.typeName != "vector" &&
      !(allowSoaVectorTarget && bindingOut.typeName == "soa_vector") &&
      !isExperimentalVectorTarget) {
    return failVectorTargetDiagnostic(std::string(helperName) + " requires vector binding");
  }
  if (!bindingOut.isMutable) {
    return failVectorTargetDiagnostic(std::string(helperName) + " requires mutable vector binding");
  }
  return true;
}

bool SemanticsValidator::resolveVectorStatementHelperTargetPath(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &receiver,
    const std::string &helperName,
    std::string &resolvedOut) {
  resolvedOut.clear();
  auto resolveExperimentalVectorReceiver = [&](const Expr &candidate,
                                               std::string &elemTypeOut) -> bool {
    BindingInfo inferredBinding;
    if (candidate.kind == Expr::Kind::Call &&
        inferBindingTypeFromInitializer(candidate, params, locals, inferredBinding) &&
        extractExperimentalVectorElementType(inferredBinding, elemTypeOut)) {
      return true;
    }
    std::string receiverTypeText;
    if (inferQueryExprTypeText(candidate, params, locals, receiverTypeText) &&
        [&]() {
          std::string base;
          std::string arg;
          const std::string normalizedType = normalizeBindingTypeName(receiverTypeText);
          if (splitTemplateTypeName(normalizedType, base, arg)) {
            inferredBinding.typeName = normalizeBindingTypeName(base);
            inferredBinding.typeTemplateArg = arg;
          } else {
            inferredBinding.typeName = normalizedType;
          }
          return extractExperimentalVectorElementType(inferredBinding, elemTypeOut);
        }()) {
      return true;
    }
    return false;
  };

  std::string experimentalElemType;
  if (resolveExperimentalVectorReceiver(receiver, experimentalElemType)) {
    resolvedOut = (helperName == "count" || helperName == "count_ref" ||
                   helperName == "capacity" ||
                   helperName == "at" || helperName == "at_unsafe")
                      ? preferredBareVectorHelperTarget(helperName)
                      : specializedExperimentalVectorHelperTarget(helperName, experimentalElemType);
    return true;
  }
  if (receiver.kind == Expr::Kind::Call &&
      (helperName == "count" || helperName == "count_ref" ||
       helperName == "capacity" ||
       helperName == "at" || helperName == "at_unsafe")) {
    std::string collectionTypePath;
    if (resolveCallCollectionTypePath(receiver, params, locals, collectionTypePath) &&
        (collectionTypePath == "/vector" || collectionTypePath == "/soa_vector")) {
      resolvedOut = collectionTypePath == "/soa_vector"
                        ? preferredSoaHelperTargetForCollectionType(helperName, "/soa_vector")
                        : preferredBareVectorHelperTarget(helperName);
      return true;
    }
  }

  BindingInfo vectorBinding;
  if (resolveVectorStatementBinding(params, locals, receiver, vectorBinding)) {
    if (extractExperimentalVectorElementType(vectorBinding, experimentalElemType)) {
      resolvedOut = specializedExperimentalVectorHelperTarget(helperName, experimentalElemType);
      return true;
    }
    if (vectorBinding.typeName == "vector" || vectorBinding.typeName == "soa_vector") {
      resolvedOut = "/" + vectorBinding.typeName + "/" + helperName;
      return true;
    }
  }

  auto resolveReceiverTypePath = [&](const std::string &typeName,
                                     const std::string &receiverNamespacePrefix) {
    if (typeName.empty()) {
      return std::string{};
    }
    if (isPrimitiveBindingTypeName(typeName)) {
      return "/" + normalizeBindingTypeName(typeName);
    }
    std::string resolvedType = resolveTypePath(typeName, receiverNamespacePrefix);
    if (structNames_.count(resolvedType) == 0 && defMap_.count(resolvedType) == 0) {
      auto importIt = importAliases_.find(typeName);
      if (importIt != importAliases_.end()) {
        resolvedType = importIt->second;
      }
    }
    return resolvedType;
  };
  auto tryResolveConcreteExperimentalSoaWrapperHelper =
      [&](const std::string &resolvedType) -> bool {
    if (!isExperimentalSoaVectorSpecializedTypePath(resolvedType) ||
        !hasVisibleSoaHelperTargetForCurrentImports(helperName)) {
      return false;
    }
    resolvedOut =
        preferredSoaHelperTargetForCollectionType(helperName, "/soa_vector");
    return true;
  };

  if (receiver.kind == Expr::Kind::Name) {
    std::string typeName;
    if (const BindingInfo *paramBinding = findParamBinding(params, receiver.name)) {
      typeName = paramBinding->typeName;
    } else {
      auto it = locals.find(receiver.name);
      if (it != locals.end()) {
        typeName = it->second.typeName;
      }
    }
    if (typeName.empty() || typeName == "Pointer" || typeName == "Reference") {
      return false;
    }
    const std::string resolvedType = resolveReceiverTypePath(typeName, receiver.namespacePrefix);
    if (resolvedType.empty()) {
      return false;
    }
    if (tryResolveConcreteExperimentalSoaWrapperHelper(resolvedType)) {
      return true;
    }
    resolvedOut = resolvedType + "/" + helperName;
    return true;
  }
  if (receiver.kind == Expr::Kind::Call && !receiver.isBinding && !receiver.isMethodCall) {
    std::string resolvedType = resolveCalleePath(receiver);
    if (resolvedType.empty() || structNames_.count(resolvedType) == 0) {
      resolvedType = inferStructReturnPath(receiver, params, locals);
    }
    if (resolvedType.empty()) {
      return false;
    }
    if (tryResolveConcreteExperimentalSoaWrapperHelper(resolvedType)) {
      return true;
    }
    resolvedOut = resolvedType + "/" + helperName;
    return true;
  }
  return false;
}

bool SemanticsValidator::isNonCollectionStructVectorHelperTarget(const std::string &resolvedPath) const {
  const size_t slash = resolvedPath.find_last_of('/');
  if (slash == std::string::npos || slash == 0) {
    return false;
  }
  const std::string receiverPath = resolvedPath.substr(0, slash);
  if (receiverPath == "/array" || receiverPath == "/vector" || receiverPath == "/map" ||
      receiverPath == "/string") {
    return false;
  }
  return structNames_.count(receiverPath) > 0;
}

bool SemanticsValidator::validateVectorStatementHelperReceiver(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &receiver,
    const std::string &helperName) {
  auto failVectorReceiverDiagnostic = [&](std::string message) -> bool {
    return failExprDiagnostic(receiver, std::move(message));
  };
  if (!validateExpr(params, locals, receiver)) {
    return false;
  }
  std::string resolvedMethod;
  if (!resolveVectorStatementHelperTargetPath(params, locals, receiver, helperName, resolvedMethod)) {
    return true;
  }
  if (defMap_.find(resolvedMethod) == defMap_.end() &&
      isNonCollectionStructVectorHelperTarget(resolvedMethod)) {
    return failVectorReceiverDiagnostic("unknown method: " + resolvedMethod);
  }
  return true;
}

} // namespace primec::semantics
