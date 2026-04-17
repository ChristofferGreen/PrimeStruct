#include "SemanticsValidator.h"

#include <string>
#include <vector>

namespace primec::semantics {
namespace {

bool isVectorCompatibilityHelperName(const std::string &helperName) {
  return helperName == "count" || helperName == "capacity" || helperName == "at" ||
         helperName == "at_unsafe" || helperName == "push" || helperName == "pop" ||
         helperName == "reserve" || helperName == "clear" || helperName == "remove_at" ||
         helperName == "remove_swap";
}

bool isStdNamespacedVectorCanonicalCompatibilityHelperPath(
    const std::string &resolved,
    const std::string &namespacedCollection,
    const std::string &namespacedHelper) {
  return resolved.rfind("/std/collections/vector/", 0) == 0 &&
         namespacedCollection == "vector" &&
         isVectorCompatibilityHelperName(namespacedHelper);
}

bool isStdNamespacedVectorCanonicalCompatibilityDirectCall(
    const Expr &expr,
    const std::string &resolved,
    const std::string &namespacedCollection,
    const std::string &namespacedHelper) {
  return !expr.isMethodCall &&
         isStdNamespacedVectorCanonicalCompatibilityHelperPath(
             resolved, namespacedCollection, namespacedHelper);
}

bool isStdNamespacedVectorCanonicalCompatibilityMethodCall(
    const Expr &expr,
    const std::string &resolved,
    const std::string &namespacedCollection,
    const std::string &namespacedHelper) {
  return expr.isMethodCall &&
         isStdNamespacedVectorCanonicalCompatibilityHelperPath(
             resolved, namespacedCollection, namespacedHelper);
}

bool isStdNamespacedVectorCanonicalDirectCallReceiverCompatible(
    const std::string &receiverFamily,
    bool hasReceiverCompatibleVisibleDefinition,
    bool isCountCapacityNamedArgException) {
  return receiverFamily == "vector" ||
         receiverFamily == "experimental_vector" ||
         hasReceiverCompatibleVisibleDefinition ||
         isCountCapacityNamedArgException;
}

bool shouldProbePositionalReorderedVectorHelperReceiver(
    const Expr &expr,
    bool isStdNamespacedVectorCanonicalCompatibilityDirectCallSite,
    bool hasNamedArgs,
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals) {
  return !isStdNamespacedVectorCanonicalCompatibilityDirectCallSite &&
         !hasNamedArgs && expr.args.size() > 1 &&
         (expr.args.front().kind == Expr::Kind::Literal ||
          expr.args.front().kind == Expr::Kind::BoolLiteral ||
          expr.args.front().kind == Expr::Kind::FloatLiteral ||
          expr.args.front().kind == Expr::Kind::StringLiteral ||
          (expr.args.front().kind == Expr::Kind::Name &&
           !isVectorHelperReceiverName(expr.args.front(), params, locals)));
}

bool isVectorHelperReceiverName(const Expr &candidate,
                                const std::vector<ParameterInfo> &params,
                                const std::unordered_map<std::string, BindingInfo> &locals) {
  if (candidate.kind != Expr::Kind::Name) {
    return false;
  }
  std::string typeName;
  for (const auto &param : params) {
    if (param.name == candidate.name) {
      typeName = normalizeBindingTypeName(param.binding.typeName);
      break;
    }
  }
  if (typeName.empty()) {
    auto it = locals.find(candidate.name);
    if (it != locals.end()) {
      typeName = normalizeBindingTypeName(it->second.typeName);
    }
  }
  return typeName == "vector" || typeName == "soa_vector" || typeName == "Vector" ||
         typeName == "/std/collections/experimental_vector/Vector" ||
         typeName == "std/collections/experimental_vector/Vector" ||
         typeName.rfind("/std/collections/experimental_vector/Vector__", 0) == 0 ||
         typeName.rfind("std/collections/experimental_vector/Vector__", 0) == 0;
}

} // namespace

bool SemanticsValidator::isVectorBuiltinName(const Expr &candidate, const char *helper) const {
  auto hasVisibleDefinitionPath = [&](const std::string &path) {
    if (hasImportedDefinitionPath(path)) {
      return true;
    }
    if (defMap_.count(path) == 0) {
      return false;
    }
    if (path.rfind("/std/collections/vector/", 0) == 0) {
      auto paramsIt = paramsByDef_.find(path);
      if (paramsIt != paramsByDef_.end() && !paramsIt->second.empty()) {
        std::string experimentalElemType;
        if (extractExperimentalVectorElementType(paramsIt->second.front().binding, experimentalElemType)) {
          return false;
        }
      }
    }
    return true;
  };
  if (isSimpleCallName(candidate, helper)) {
    return true;
  }
  std::string namespacedCollection;
  std::string namespacedHelper;
  if (!getNamespacedCollectionHelperName(candidate, namespacedCollection, namespacedHelper)) {
    return false;
  }
  if (!(namespacedCollection == "vector" && namespacedHelper == helper)) {
    const std::string resolvedPath = resolveCalleePath(candidate);
    if (resolvedPath.rfind("/std/", 0) != 0 || defMap_.count(resolvedPath) != 0) {
      return false;
    }
    const std::string helperName = helper == nullptr ? std::string() : std::string(helper);
    const size_t lastSlash = resolvedPath.find_last_of('/');
    if (lastSlash == std::string::npos || resolvedPath.substr(lastSlash + 1) != helperName) {
      return false;
    }
    return isVectorCompatibilityHelperName(helperName);
  }
  if ((namespacedHelper != "count" && namespacedHelper != "capacity") ||
      resolveCalleePath(candidate) != "/std/collections/vector/" + namespacedHelper) {
    return true;
  }
  return hasVisibleDefinitionPath("/std/collections/vector/" + namespacedHelper);
}

bool SemanticsValidator::resolveVectorHelperMethodTarget(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &receiver,
    const std::string &helperName,
    std::string &resolvedOut) {
  resolvedOut.clear();
  auto explicitCallPath = [](const Expr &callExpr) -> std::string {
    if (callExpr.kind != Expr::Kind::Call || callExpr.isMethodCall ||
        callExpr.name.empty()) {
      return {};
    }
    if (callExpr.name.front() == '/') {
      return callExpr.name;
    }
    std::string prefix = callExpr.namespacePrefix;
    if (!prefix.empty() && prefix.front() != '/') {
      prefix.insert(prefix.begin(), '/');
    }
    if (prefix.empty()) {
      return "/" + callExpr.name;
    }
    return prefix + "/" + callExpr.name;
  };
  auto isRootMapAliasPath = [](const std::string &path) {
    return path == "/map" ||
           path.rfind("/map__", 0) == 0;
  };
  auto isLocalRootMapAliasCall = [&](const Expr &candidate) {
    if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall) {
      return false;
    }
    return isRootMapAliasPath(resolveCalleePath(candidate)) ||
           isRootMapAliasPath(explicitCallPath(candidate));
  };
  auto resolveExperimentalVectorReceiver = [&](const Expr &candidate,
                                               std::string &elemTypeOut) -> bool {
    BindingInfo inferredBinding;
    if (candidate.kind == Expr::Kind::Name) {
      if (const BindingInfo *paramBinding = findParamBinding(params, candidate.name)) {
        return extractExperimentalVectorElementType(*paramBinding, elemTypeOut);
      }
      auto localIt = locals.find(candidate.name);
      if (localIt != locals.end()) {
        return extractExperimentalVectorElementType(localIt->second, elemTypeOut);
      }
    }
    if (candidate.kind == Expr::Kind::Call) {
      std::string collectionTypePath;
      if (resolveCallCollectionTypePath(candidate, params, locals, collectionTypePath) &&
          (collectionTypePath == "/vector" || collectionTypePath == "/soa_vector" ||
           collectionTypePath == "/map")) {
        return false;
      }
      auto defIt = defMap_.find(resolveCalleePath(candidate));
      if (defIt != defMap_.end() && defIt->second != nullptr &&
          inferDefinitionReturnBinding(*defIt->second, inferredBinding) &&
          extractExperimentalVectorElementType(inferredBinding, elemTypeOut)) {
        return true;
      }
      if (inferBindingTypeFromInitializer(candidate, params, locals, inferredBinding) &&
          extractExperimentalVectorElementType(inferredBinding, elemTypeOut)) {
        return true;
      }
    }
    std::string receiverTypeText;
    if (inferQueryExprTypeText(candidate, params, locals, receiverTypeText)) {
      std::string base;
      std::string argText;
      const std::string normalizedType = normalizeBindingTypeName(receiverTypeText);
      if (splitTemplateTypeName(normalizedType, base, argText)) {
        inferredBinding.typeName = normalizeBindingTypeName(base);
        inferredBinding.typeTemplateArg = argText;
      } else {
        inferredBinding.typeName = normalizedType;
        inferredBinding.typeTemplateArg.clear();
      }
      if (extractExperimentalVectorElementType(inferredBinding, elemTypeOut)) {
        return true;
      }
    }
    return false;
  };
  auto resolveExperimentalSoaVectorReceiver = [&](const Expr &candidate,
                                                  std::string &elemTypeOut) -> bool {
    BindingInfo inferredBinding;
    if (candidate.kind == Expr::Kind::Name) {
      if (const BindingInfo *paramBinding = findParamBinding(params, candidate.name)) {
        return extractExperimentalSoaVectorElementType(*paramBinding, elemTypeOut);
      }
      auto localIt = locals.find(candidate.name);
      if (localIt != locals.end()) {
        return extractExperimentalSoaVectorElementType(localIt->second, elemTypeOut);
      }
    }
    if (candidate.kind == Expr::Kind::Call) {
      std::string collectionTypePath;
      if (resolveCallCollectionTypePath(candidate, params, locals, collectionTypePath) &&
          (collectionTypePath == "/vector" || collectionTypePath == "/soa_vector" ||
           collectionTypePath == "/map")) {
        return false;
      }
      auto defIt = defMap_.find(resolveCalleePath(candidate));
      if (defIt != defMap_.end() && defIt->second != nullptr &&
          inferDefinitionReturnBinding(*defIt->second, inferredBinding) &&
          extractExperimentalSoaVectorElementType(inferredBinding, elemTypeOut)) {
        return true;
      }
      if (inferBindingTypeFromInitializer(candidate, params, locals, inferredBinding) &&
          extractExperimentalSoaVectorElementType(inferredBinding, elemTypeOut)) {
        return true;
      }
    }
    std::string receiverTypeText;
    if (inferQueryExprTypeText(candidate, params, locals, receiverTypeText)) {
      std::string base;
      std::string argText;
      const std::string normalizedType = normalizeBindingTypeName(receiverTypeText);
      if (splitTemplateTypeName(normalizedType, base, argText)) {
        inferredBinding.typeName = normalizeBindingTypeName(base);
        inferredBinding.typeTemplateArg = argText;
      } else {
        inferredBinding.typeName = normalizedType;
        inferredBinding.typeTemplateArg.clear();
      }
      if (extractExperimentalSoaVectorElementType(inferredBinding, elemTypeOut)) {
        return true;
      }
    }
    return false;
  };
  std::string experimentalElemType;
  if (resolveExperimentalVectorReceiver(receiver, experimentalElemType)) {
    if ((helperName == "at" || helperName == "at_unsafe") &&
        (hasDeclaredDefinitionPath("/std/collections/vector/" + helperName) ||
         hasImportedDefinitionPath("/std/collections/vector/" + helperName))) {
      resolvedOut = preferredBareVectorHelperTarget(helperName);
      return true;
    }
    resolvedOut = specializedExperimentalVectorHelperTarget(helperName, experimentalElemType);
    return true;
  }
  std::string experimentalSoaElemType;
  if (resolveExperimentalSoaVectorReceiver(receiver, experimentalSoaElemType) &&
      (helperName == "push" || helperName == "reserve")) {
    resolvedOut =
        preferredSoaHelperTargetForCollectionType(helperName, "/soa_vector");
    return true;
  }
  if (receiver.kind == Expr::Kind::Call &&
      (helperName == "count" || helperName == "capacity" ||
       helperName == "at" || helperName == "at_unsafe" || helperName == "insert" ||
       helperName == "push" || helperName == "reserve")) {
    std::string collectionTypePath;
    if (resolveCallCollectionTypePath(receiver, params, locals, collectionTypePath)) {
      if (collectionTypePath == "/vector" &&
          (helperName == "push" || helperName == "reserve") &&
          usesSamePathSoaHelperTargetForCollectionType(helperName,
                                                       "/vector")) {
        resolvedOut =
            preferredSoaHelperTargetForCollectionType(helperName, "/vector");
        return true;
      }
      if (collectionTypePath == "/vector") {
        resolvedOut = preferredBareVectorHelperTarget(helperName);
        return true;
      }
      if (collectionTypePath == "/soa_vector" &&
          (helperName == "push" || helperName == "reserve")) {
        resolvedOut =
            preferredSoaHelperTargetForCollectionType(helperName, "/soa_vector");
        return true;
      }
      if (collectionTypePath == "/map" &&
          !isLocalRootMapAliasCall(receiver) &&
          (helperName == "count" || helperName == "count_ref" ||
           helperName == "contains" || helperName == "contains_ref" ||
           helperName == "tryAt" || helperName == "tryAt_ref" ||
           helperName == "at" || helperName == "at_ref" ||
           helperName == "at_unsafe" || helperName == "at_unsafe_ref" ||
           helperName == "insert" || helperName == "insert_ref")) {
        resolvedOut = preferredBareMapHelperTarget(helperName);
        return true;
      }
    }
  }
  auto resolveReceiverTypePath = [&](const std::string &typeName,
                                     const std::string &namespacePrefix) -> std::string {
    if (typeName.empty()) {
      return "";
    }
    if (isPrimitiveBindingTypeName(typeName)) {
      return "/" + normalizeBindingTypeName(typeName);
    }
    std::string resolvedType = resolveTypePath(typeName, namespacePrefix);
    if (structNames_.count(resolvedType) == 0 && defMap_.count(resolvedType) == 0) {
      auto importIt = importAliases_.find(typeName);
      if (importIt != importAliases_.end()) {
        resolvedType = importIt->second;
      }
    }
    return resolvedType;
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
    const std::string normalizedTypeName = normalizeBindingTypeName(typeName);
    const std::string resolvedType = resolveReceiverTypePath(typeName, receiver.namespacePrefix);
    if (resolvedType.empty()) {
      return false;
    }
    if ((resolvedType == "/map" || isMapCollectionTypeName(normalizedTypeName)) &&
        (helperName == "count" || helperName == "count_ref" ||
         helperName == "contains" || helperName == "contains_ref" ||
         helperName == "tryAt" || helperName == "tryAt_ref" ||
         helperName == "at" || helperName == "at_ref" ||
         helperName == "at_unsafe" || helperName == "at_unsafe_ref" ||
         helperName == "insert" || helperName == "insert_ref")) {
      resolvedOut = preferredBareMapHelperTarget(helperName);
      return true;
    }
    if ((resolvedType == "/vector" || normalizedTypeName == "vector") &&
        (helperName == "push" || helperName == "reserve") &&
        usesSamePathSoaHelperTargetForCollectionType(helperName, "/vector")) {
      resolvedOut =
          preferredSoaHelperTargetForCollectionType(helperName, "/vector");
      return true;
    }
    if ((resolvedType == "/soa_vector" || normalizedTypeName == "soa_vector") &&
        (helperName == "push" || helperName == "reserve")) {
      resolvedOut =
          preferredSoaHelperTargetForCollectionType(helperName, "/soa_vector");
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
    if (!resolvedType.empty()) {
      if ((resolvedType == "/vector" || normalizeBindingTypeName(resolvedType) == "vector") &&
          (helperName == "push" || helperName == "reserve") &&
          usesSamePathSoaHelperTargetForCollectionType(helperName,
                                                       "/vector")) {
        resolvedOut =
            preferredSoaHelperTargetForCollectionType(helperName, "/vector");
        return true;
      }
      if (isVectorCompatibilityHelperName(helperName)) {
        BindingInfo receiverBinding;
        receiverBinding.typeName = resolvedType;
        std::string experimentalElemType;
        if (extractExperimentalVectorElementType(receiverBinding, experimentalElemType)) {
          if ((helperName == "at" || helperName == "at_unsafe") &&
              (hasDeclaredDefinitionPath("/std/collections/vector/" + helperName) ||
               hasImportedDefinitionPath("/std/collections/vector/" + helperName))) {
            resolvedOut = preferredBareVectorHelperTarget(helperName);
            return true;
          }
          resolvedOut = specializedExperimentalVectorHelperTarget(helperName, experimentalElemType);
          return true;
        }
      }
      resolvedOut = resolvedType + "/" + helperName;
      return true;
    }
    const ReturnKind receiverKind = inferExprReturnKind(receiver, params, locals);
    if (receiverKind == ReturnKind::Unknown || receiverKind == ReturnKind::Void ||
        receiverKind == ReturnKind::Array) {
      return false;
    }
    const std::string receiverType = typeNameForReturnKind(receiverKind);
    if (receiverType.empty()) {
      return false;
    }
    resolvedOut = "/" + receiverType + "/" + helperName;
    return true;
  }
  return false;
}

bool SemanticsValidator::resolveExprVectorHelperCall(const std::vector<ParameterInfo> &params,
                                                     const std::unordered_map<std::string, BindingInfo> &locals,
                                                     const Expr &expr,
                                                     bool &hasResolutionOut,
                                                     std::string &resolvedPathOut,
                                                     size_t &receiverIndexOut) {
  auto failVectorHelperDiagnostic = [&](std::string message) -> bool {
    return failExprDiagnostic(expr, std::move(message));
  };
  hasResolutionOut = false;
  resolvedPathOut.clear();
  receiverIndexOut = 0;

  std::string vectorHelper;
  if (!getVectorStatementHelperName(expr, vectorHelper)) {
    return true;
  }
  auto hasVisibleDefinitionPath = [&](const std::string &path) {
    if (hasImportedDefinitionPath(path)) {
      return true;
    }
    if (defMap_.count(path) == 0) {
      return false;
    }
    if (path.rfind("/std/collections/vector/", 0) == 0) {
      auto paramsIt = paramsByDef_.find(path);
      if (paramsIt != paramsByDef_.end() && !paramsIt->second.empty()) {
        std::string experimentalElemType;
        if (extractExperimentalVectorElementType(paramsIt->second.front().binding, experimentalElemType)) {
          return false;
        }
      }
    }
    return true;
  };
  auto classifyReceiverFamily = [&](const Expr &candidate) -> std::string {
    BindingInfo receiverBinding;
    auto assignBindingFromTypeText = [&](const std::string &typeText) {
      std::string base;
      std::string argText;
      const std::string normalizedType = normalizeBindingTypeName(typeText);
      if (splitTemplateTypeName(normalizedType, base, argText)) {
        receiverBinding.typeName = normalizeBindingTypeName(base);
        receiverBinding.typeTemplateArg = argText;
      } else {
        receiverBinding.typeName = normalizedType;
        receiverBinding.typeTemplateArg.clear();
      }
    };
    if (candidate.kind == Expr::Kind::Name) {
      if (const BindingInfo *paramBinding = findParamBinding(params, candidate.name)) {
        receiverBinding = *paramBinding;
      } else if (auto it = locals.find(candidate.name); it != locals.end()) {
        receiverBinding = it->second;
      }
    }
    if (receiverBinding.typeName.empty()) {
      if (!inferBindingTypeFromInitializer(candidate, params, locals, receiverBinding)) {
        std::string receiverTypeText;
        if (!inferQueryExprTypeText(candidate, params, locals, receiverTypeText)) {
          return {};
        }
        assignBindingFromTypeText(receiverTypeText);
      }
    }
    std::string elemType;
    std::string keyType;
    std::string valueType;
    if (extractExperimentalVectorElementType(receiverBinding, elemType)) {
      return "experimental_vector";
    }
    if (extractMapKeyValueTypes(receiverBinding, keyType, valueType)) {
      return "map";
    }
    const std::string normalizedBase = normalizeBindingTypeName(receiverBinding.typeName);
    if (normalizedBase == "vector" || normalizedBase == "soa_vector" ||
        normalizedBase == "array" || normalizedBase == "string") {
      return normalizedBase;
    }
    return {};
  };
  auto hasReceiverCompatibleVisibleDefinitionPath = [&](const std::string &path, const Expr &receiverExpr) {
    if (!hasVisibleDefinitionPath(path)) {
      return false;
    }
    auto paramsIt = paramsByDef_.find(path);
    if (paramsIt == paramsByDef_.end() || paramsIt->second.empty()) {
      return false;
    }
    const std::string receiverFamily = classifyReceiverFamily(receiverExpr);
    if (receiverFamily.empty()) {
      return false;
    }
    const BindingInfo &binding = paramsIt->second.front().binding;
    std::string elemType;
    std::string keyType;
    std::string valueType;
    if (extractExperimentalVectorElementType(binding, elemType)) {
      return receiverFamily == "experimental_vector";
    }
    if (extractMapKeyValueTypes(binding, keyType, valueType)) {
      return receiverFamily == "map";
    }
    const std::string normalizedBase = normalizeBindingTypeName(binding.typeName);
    return normalizedBase == receiverFamily;
  };

  std::string resolved = resolveCalleePath(expr);
  std::string namespacedCollection;
  std::string namespacedHelper;
  getNamespacedCollectionHelperName(expr, namespacedCollection, namespacedHelper);
  const bool hasNamedArgs = hasNamedArguments(expr.argNames);
  const bool isStdNamespacedVectorCanonicalCompatibilityDirectCallSite =
      isStdNamespacedVectorCanonicalCompatibilityDirectCall(
          expr, resolved, namespacedCollection, namespacedHelper);
  const bool isStdNamespacedVectorCanonicalCountCapacityNamedArgException =
      isStdNamespacedVectorCanonicalCompatibilityDirectCallSite &&
      (namespacedHelper == "count" || namespacedHelper == "capacity") &&
      hasNamedArgs;
  if (isStdNamespacedVectorCanonicalCompatibilityDirectCallSite &&
      !hasVisibleDefinitionPath(resolved) &&
      !isStdNamespacedVectorCanonicalCountCapacityNamedArgException) {
    return failVectorHelperDiagnostic("unknown call target: " + resolved);
  }
  if (isStdNamespacedVectorCanonicalCompatibilityMethodCall(
          expr, resolved, namespacedCollection, namespacedHelper)) {
    return true;
  }
  if (isStdNamespacedVectorCanonicalCompatibilityDirectCallSite &&
      expr.args.size() == 1 &&
      !expr.hasBodyArguments &&
      expr.bodyArguments.empty()) {
    const Expr &receiverExpr = expr.args.front();
    const std::string receiverFamily = classifyReceiverFamily(receiverExpr);
    if (!isStdNamespacedVectorCanonicalDirectCallReceiverCompatible(
            receiverFamily,
            defMap_.count(resolved) > 0 &&
                hasReceiverCompatibleVisibleDefinitionPath(resolved, receiverExpr),
            isStdNamespacedVectorCanonicalCountCapacityNamedArgException)) {
      return failVectorHelperDiagnostic("unknown call target: " + resolved);
    }
  }

  const bool resolvedVectorHelperDefinitionMissing =
      defMap_.find(resolved) == defMap_.end();
  size_t namedValuesReceiverIndex = expr.args.size();
  const bool hasNamedValuesReceiver = [&]() {
    if (!hasNamedArgs) {
      return false;
    }
    for (size_t i = 0; i < expr.args.size(); ++i) {
      if (i < expr.argNames.size() && expr.argNames[i].has_value() &&
          *expr.argNames[i] == "values") {
        namedValuesReceiverIndex = i;
        return true;
      }
    }
    return false;
  }();
  size_t resolvedReceiverIndex = 0;
  if ((!isStdNamespacedVectorCanonicalCompatibilityDirectCallSite ||
       isStdNamespacedVectorCanonicalCountCapacityNamedArgException) &&
      resolvedVectorHelperDefinitionMissing &&
      !expr.args.empty()) {
    auto tryResolveReceiverIndex = [&](size_t receiverIndex) {
      if (receiverIndex >= expr.args.size()) {
        return false;
      }
      const Expr &receiverCandidate = expr.args[receiverIndex];
      std::string methodTarget;
      if (resolveVectorHelperMethodTarget(params, locals, receiverCandidate, vectorHelper, methodTarget)) {
        if (!expr.isMethodCall) {
          methodTarget = preferVectorStdlibHelperPath(methodTarget);
        }
      }
      if (!hasVisibleDefinitionPath(methodTarget)) {
        return false;
      }
      resolved = methodTarget;
      resolvedReceiverIndex = receiverIndex;
      return true;
    };

    bool resolvedReceiver = false;
    auto tryResolveRemainingReceivers = [&](size_t startIndex) {
      for (size_t i = startIndex; i < expr.args.size(); ++i) {
        if (tryResolveReceiverIndex(i)) {
          resolvedReceiver = true;
          return true;
        }
      }
      return false;
    };
    auto tryResolvePrimaryReceiver = [&]() {
      resolvedReceiver = tryResolveReceiverIndex(0);
      return resolvedReceiver;
    };
    auto tryResolveNamedValuesReceiver = [&]() {
      if (!hasNamedValuesReceiver) {
        return false;
      }
      resolvedReceiver = tryResolveReceiverIndex(namedValuesReceiverIndex);
      return resolvedReceiver;
    };
    if (hasNamedArgs) {
      auto tryResolveNamedFallbackReceivers = [&]() {
        if (hasNamedValuesReceiver || resolvedReceiver) {
          return;
        }
        if (tryResolvePrimaryReceiver()) {
          return;
        }
        tryResolveRemainingReceivers(1);
      };
      tryResolveNamedValuesReceiver();
      tryResolveNamedFallbackReceivers();
    } else {
      tryResolvePrimaryReceiver();
    }

    if (shouldProbePositionalReorderedVectorHelperReceiver(
            expr, isStdNamespacedVectorCanonicalCompatibilityDirectCallSite,
            hasNamedArgs, params, locals) &&
        !resolvedReceiver) {
      tryResolveRemainingReceivers(1);
    }
  }

  if (resolvedVectorHelperDefinitionMissing) {
    if (!isStdNamespacedVectorCanonicalCompatibilityDirectCallSite) {
      return failVectorHelperDiagnostic(vectorHelper + " is only supported as a statement");
    }
    return true;
  }

  hasResolutionOut = true;
  resolvedPathOut = resolved;
  receiverIndexOut = resolvedReceiverIndex;
  return true;
}

} // namespace primec::semantics
