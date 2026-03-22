#include "SemanticsValidator.h"

#include <functional>

namespace primec::semantics {

bool SemanticsValidator::resolveInferMethodCallPath(
    const Expr &expr,
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const std::string &methodName,
    std::string &resolvedOut) {
  auto resolveArgsPackCountTarget = [&](const Expr &target, std::string &elemType) -> bool {
    return resolveInferArgsPackCountTarget(params, locals, target, elemType);
  };
  const BuiltinCollectionDispatchResolverAdapters builtinCollectionDispatchResolverAdapters;
  const BuiltinCollectionDispatchResolvers builtinCollectionDispatchResolvers =
      makeBuiltinCollectionDispatchResolvers(params, locals, builtinCollectionDispatchResolverAdapters);
  const auto &resolveArgsPackAccessTarget = builtinCollectionDispatchResolvers.resolveArgsPackAccessTarget;
  const auto &resolveArrayTarget = builtinCollectionDispatchResolvers.resolveArrayTarget;
  const auto &resolveVectorTarget = builtinCollectionDispatchResolvers.resolveVectorTarget;
  const auto &resolveSoaVectorTarget = builtinCollectionDispatchResolvers.resolveSoaVectorTarget;
  const auto &resolveStringTarget = builtinCollectionDispatchResolvers.resolveStringTarget;
  const auto &resolveMapTarget = builtinCollectionDispatchResolvers.resolveMapTarget;
  auto resolveExperimentalMapTarget = [&](const Expr &target,
                                          std::string &keyTypeOut,
                                          std::string &valueTypeOut) -> bool {
    return resolveInferExperimentalMapTarget(params, locals, target, keyTypeOut, valueTypeOut);
  };

  auto inferCollectionTypePathFromTypeText = [&](const std::string &typeText,
                                                  auto &&inferCollectionTypePathFromTypeTextRef)
      -> std::string {
    const std::string normalizedType = normalizeBindingTypeName(typeText);
    std::string normalizedCollectionType = normalizeCollectionTypePath(normalizedType);
    if (!normalizedCollectionType.empty()) {
      return normalizedCollectionType;
    }
    std::string base;
    std::string argText;
    if (!splitTemplateTypeName(normalizedType, base, argText)) {
      return {};
    }
    base = normalizeBindingTypeName(base);
    if (base == "Reference" || base == "Pointer") {
      std::vector<std::string> args;
      if (!splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
        return {};
      }
      return inferCollectionTypePathFromTypeTextRef(args.front(), inferCollectionTypePathFromTypeTextRef);
    }
    std::vector<std::string> args;
    if (!splitTopLevelTemplateArgs(argText, args)) {
      return {};
    }
    if ((base == "array" || base == "vector" || base == "soa_vector") && args.size() == 1) {
      return "/" + base;
    }
    if (isMapCollectionTypeName(base) && args.size() == 2) {
      return "/map";
    }
    return {};
  };
  auto resolveStructTypePathForMethod = [&](const std::string &typeName,
                                            const std::string &namespacePrefix) -> std::string {
    if (typeName.empty()) {
      return "";
    }
    if (!typeName.empty() && typeName[0] == '/') {
      return typeName;
    }
    std::string current = namespacePrefix;
    while (true) {
      if (!current.empty()) {
        std::string scoped = current + "/" + typeName;
        if (structNames_.count(scoped) > 0) {
          return scoped;
        }
        if (current.size() > typeName.size()) {
          const size_t start = current.size() - typeName.size();
          if (start > 0 && current[start - 1] == '/' &&
              current.compare(start, typeName.size(), typeName) == 0 &&
              structNames_.count(current) > 0) {
            return current;
          }
        }
      } else {
        std::string root = "/" + typeName;
        if (structNames_.count(root) > 0) {
          return root;
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
    auto importIt = importAliases_.find(typeName);
    if (importIt != importAliases_.end()) {
      return importIt->second;
    }
    return "";
  };

  if (expr.args.empty()) {
    return false;
  }
  const Expr &receiver = expr.args.front();
  std::string typeName;
  std::string typeTemplateArg;
  std::string normalizedMethodName = methodName;
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
  auto inferPointerLikeCallReturnType = [&](const Expr &receiverExpr) -> std::string {
    if (receiverExpr.kind != Expr::Kind::Call || receiverExpr.isBinding || receiverExpr.isMethodCall) {
      return "";
    }
    return this->inferPointerLikeCallReturnType(receiverExpr, params, locals);
  };
  auto preferredMapMethodTargetForCall = [&](const Expr &receiverExpr, const std::string &helperName) {
    std::string keyType;
    std::string valueType;
    if (resolveExperimentalMapTarget(receiverExpr, keyType, valueType)) {
      const std::string canonical = "/std/collections/map/" + helperName;
      if (hasDefinitionPath(canonical) || hasImportedDefinitionPath(canonical)) {
        return canonical;
      }
      return preferredCanonicalExperimentalMapHelperTarget(helperName);
    }
    const std::string canonical = "/std/collections/map/" + helperName;
    const std::string alias = "/map/" + helperName;
    if (hasDefinitionPath(canonical) || hasImportedDefinitionPath(canonical)) {
      return canonical;
    }
    if (hasDefinitionPath(alias)) {
      return alias;
    }
    return canonical;
  };
  auto resolveCollectionMethodFromTypePath = [&](const std::string &collectionTypePath) -> bool {
    if (normalizedMethodName == "count") {
      if (collectionTypePath == "/array") {
        resolvedOut = preferVectorStdlibHelperPath("/array/count");
        return true;
      }
      if (collectionTypePath == "/vector") {
        resolvedOut = preferredBareVectorHelperTarget("count");
        return true;
      }
      if (collectionTypePath == "/soa_vector") {
        resolvedOut = "/soa_vector/count";
        return true;
      }
      if (collectionTypePath == "/string") {
        resolvedOut = "/string/count";
        return true;
      }
      if (collectionTypePath == "/map") {
        resolvedOut = preferredMapMethodTargetForCall(receiver, "count");
        return true;
      }
    }
    if (normalizedMethodName == "capacity" && collectionTypePath == "/vector") {
      resolvedOut = preferredBareVectorHelperTarget("capacity");
      return true;
    }
    if (normalizedMethodName == "contains" && collectionTypePath == "/map") {
      resolvedOut = preferredMapMethodTargetForCall(receiver, "contains");
      return true;
    }
    if (normalizedMethodName == "tryAt" && collectionTypePath == "/map") {
      resolvedOut = preferredMapMethodTargetForCall(receiver, "tryAt");
      return true;
    }
    if (normalizedMethodName == "at" || normalizedMethodName == "at_unsafe") {
      if (collectionTypePath == "/array") {
        resolvedOut = preferVectorStdlibHelperPath("/array/" + normalizedMethodName);
        return true;
      }
      if (collectionTypePath == "/vector") {
        resolvedOut = preferVectorStdlibHelperPath("/vector/" + normalizedMethodName);
        return true;
      }
      if (collectionTypePath == "/string") {
        resolvedOut = "/string/" + normalizedMethodName;
        return true;
      }
      if (collectionTypePath == "/map") {
        resolvedOut = preferredMapMethodTargetForCall(receiver, normalizedMethodName);
        return true;
      }
    }
    if ((normalizedMethodName == "get" || normalizedMethodName == "ref") &&
        collectionTypePath == "/soa_vector") {
      resolvedOut = "/soa_vector/" + normalizedMethodName;
      return true;
    }
    return false;
  };
  auto setIndexedArgsPackMapMethodTarget = [&](const Expr &receiverExpr, const std::string &helperName) -> bool {
    if (receiverExpr.kind != Expr::Kind::Call || receiverExpr.isBinding || receiverExpr.args.size() != 2) {
      return false;
    }
    std::string accessName;
    if (!getBuiltinArrayAccessName(receiverExpr, accessName)) {
      return false;
    }
    const Expr *accessReceiver = this->resolveBuiltinAccessReceiverExpr(receiverExpr);
    if (accessReceiver == nullptr) {
      return false;
    }
    std::string indexedElemType;
    std::string keyType;
    std::string valueType;
    if (!resolveArgsPackAccessTarget(*accessReceiver, indexedElemType) ||
        !extractMapKeyValueTypesFromTypeText(indexedElemType, keyType, valueType)) {
      return false;
    }
    resolvedOut = preferredMapMethodTargetForCall(receiverExpr, helperName);
    return true;
  };
  auto shouldPreferStructReturnMethodTargetForCall = [&](const Expr &receiverExpr) {
    return receiverExpr.kind == Expr::Kind::Call &&
           !receiverExpr.isBinding &&
           receiverExpr.isFieldAccess;
  };

  if (receiver.kind == Expr::Kind::Call && !receiver.isBinding && !receiver.isMethodCall) {
    std::string resolvedType = resolveCalleePath(receiver);
    if (!resolvedType.empty() && structNames_.count(resolvedType) > 0) {
      resolvedOut = resolvedType + "/" + normalizedMethodName;
      return true;
    }
    std::string receiverTypeText;
    if (inferQueryExprTypeText(receiver, params, locals, receiverTypeText) &&
        !receiverTypeText.empty()) {
      const std::string normalizedReceiverType = normalizeBindingTypeName(receiverTypeText);
      const std::string receiverCollectionTypePath =
          inferCollectionTypePathFromTypeText(normalizedReceiverType, inferCollectionTypePathFromTypeText);
      if (!receiverCollectionTypePath.empty() &&
          resolveCollectionMethodFromTypePath(receiverCollectionTypePath)) {
        return true;
      }
      const std::string resolvedReceiverType =
          resolveStructTypePathForMethod(normalizedReceiverType, receiver.namespacePrefix);
      if (!resolvedReceiverType.empty()) {
        resolvedOut = resolvedReceiverType + "/" + normalizedMethodName;
        return true;
      }
      if (isPrimitiveBindingTypeName(normalizedReceiverType)) {
        resolvedOut = "/" + normalizedReceiverType + "/" + normalizedMethodName;
        return true;
      }
    }
    resolvedType = inferStructReturnPath(receiver, params, locals);
    if (!resolvedType.empty()) {
      resolvedOut = resolvedType + "/" + normalizedMethodName;
      return true;
    }
    const ReturnKind receiverKind = inferExprReturnKind(receiver, params, locals);
    if (receiverKind == ReturnKind::Unknown || receiverKind == ReturnKind::Void) {
      return false;
    }
    if (receiverKind != ReturnKind::Array) {
      const std::string receiverType = typeNameForReturnKind(receiverKind);
      if (receiverType.empty()) {
        return false;
      }
      resolvedOut = "/" + receiverType + "/" + normalizedMethodName;
      return true;
    }
  }
  if (receiver.kind == Expr::Kind::Call && !receiver.isBinding &&
      shouldPreferStructReturnMethodTargetForCall(receiver)) {
    std::string inferredStructPath = inferStructReturnPath(receiver, params, locals);
    if (!inferredStructPath.empty() && structNames_.count(inferredStructPath) > 0) {
      resolvedOut = inferredStructPath + "/" + normalizedMethodName;
      return true;
    }
  } else if (receiver.kind == Expr::Kind::Call && !receiver.isBinding &&
             receiver.isMethodCall && !receiver.args.empty()) {
    std::string receiverHelperName = receiver.name;
    if (!receiverHelperName.empty() && receiverHelperName.front() == '/') {
      receiverHelperName.erase(receiverHelperName.begin());
    }
    if (receiverHelperName.rfind("map/", 0) == 0) {
      receiverHelperName.erase(0, std::string("map/").size());
    } else if (receiverHelperName.rfind("std/collections/map/", 0) == 0) {
      receiverHelperName.erase(0, std::string("std/collections/map/").size());
    }
    std::string keyType;
    std::string valueType;
    if ((receiverHelperName == "at" || receiverHelperName == "at_unsafe") &&
        resolveMapTarget(receiver.args.front(), keyType, valueType)) {
      const std::string resolvedReceiver =
          preferredMapMethodTargetForCall(receiver, receiverHelperName);
      auto defIt = defMap_.find(resolvedReceiver);
      if (defIt != defMap_.end() && defIt->second != nullptr) {
        BindingInfo inferredReceiverBinding;
        if (inferDefinitionReturnBinding(*defIt->second, inferredReceiverBinding)) {
          const std::string receiverTypeText =
              inferredReceiverBinding.typeTemplateArg.empty()
                  ? inferredReceiverBinding.typeName
                  : inferredReceiverBinding.typeName + "<" + inferredReceiverBinding.typeTemplateArg + ">";
          const std::string resolvedReceiverType =
              resolveStructTypePathForMethod(normalizeBindingTypeName(receiverTypeText), defIt->second->namespacePrefix);
          if (!resolvedReceiverType.empty()) {
            resolvedOut = resolvedReceiverType + "/" + normalizedMethodName;
            return true;
          }
        }
      }
    }
  }
  if (receiver.kind == Expr::Kind::Call) {
    std::string receiverCollectionTypePath;
    if (resolveCallCollectionTypePath(receiver, params, locals, receiverCollectionTypePath) &&
        resolveCollectionMethodFromTypePath(receiverCollectionTypePath)) {
      return true;
    }
  }
  if (receiver.kind == Expr::Kind::Name &&
      findParamBinding(params, receiver.name) == nullptr &&
      locals.find(receiver.name) == locals.end()) {
    std::string resolvedReceiverPath;
    const std::string rootReceiverPath = "/" + receiver.name;
    if (defMap_.find(rootReceiverPath) != defMap_.end()) {
      resolvedReceiverPath = rootReceiverPath;
    } else {
      auto importIt = importAliases_.find(receiver.name);
      if (importIt != importAliases_.end()) {
        resolvedReceiverPath = importIt->second;
      }
    }
    if (!resolvedReceiverPath.empty() &&
        (structNames_.count(resolvedReceiverPath) > 0 ||
         defMap_.find(resolvedReceiverPath + "/" + normalizedMethodName) != defMap_.end())) {
      resolvedOut = resolvedReceiverPath + "/" + normalizedMethodName;
      return true;
    }
    const std::string resolvedType = resolveStructTypePathForMethod(receiver.name, receiver.namespacePrefix);
    if (!resolvedType.empty()) {
      resolvedOut = resolvedType + "/" + normalizedMethodName;
      return true;
    }
  }
  {
    std::string elemType;
    std::string keyType;
    std::string valueType;
    if (normalizedMethodName == "count") {
      if (resolveArgsPackCountTarget(receiver, elemType)) {
        resolvedOut = preferVectorStdlibHelperPath("/array/count");
        return true;
      }
      if (resolveVectorTarget(receiver, elemType)) {
        resolvedOut = preferredBareVectorHelperTarget("count");
        return true;
      }
      if (resolveSoaVectorTarget(receiver, elemType)) {
        resolvedOut = "/soa_vector/count";
        return true;
      }
      if (resolveArrayTarget(receiver, elemType)) {
        resolvedOut = preferVectorStdlibHelperPath("/array/count");
        return true;
      }
      if (resolveStringTarget(receiver)) {
        resolvedOut = "/string/count";
        return true;
      }
      if (setIndexedArgsPackMapMethodTarget(receiver, "count")) {
        return true;
      }
    }
    if (normalizedMethodName == "capacity" && resolveVectorTarget(receiver, elemType)) {
      resolvedOut = preferredBareVectorHelperTarget("capacity");
      return true;
    }
    if (normalizedMethodName == "count" && resolveMapTarget(receiver, keyType, valueType)) {
      resolvedOut = preferredMapMethodTargetForCall(receiver, "count");
      return true;
    }
    if ((normalizedMethodName == "contains" || normalizedMethodName == "tryAt") &&
        resolveMapTarget(receiver, keyType, valueType)) {
      resolvedOut = preferredMapMethodTargetForCall(receiver, normalizedMethodName);
      return true;
    }
    if ((normalizedMethodName == "contains" || normalizedMethodName == "tryAt") &&
        setIndexedArgsPackMapMethodTarget(receiver, normalizedMethodName)) {
      return true;
    }
    if (normalizedMethodName == "at" || normalizedMethodName == "at_unsafe") {
      if (resolveArgsPackAccessTarget(receiver, elemType)) {
        resolvedOut = preferVectorStdlibHelperPath("/array/" + normalizedMethodName);
        return true;
      }
      if (resolveVectorTarget(receiver, elemType)) {
        resolvedOut = preferVectorStdlibHelperPath("/vector/" + normalizedMethodName);
        return true;
      }
      if (resolveArrayTarget(receiver, elemType)) {
        resolvedOut = preferVectorStdlibHelperPath("/array/" + normalizedMethodName);
        return true;
      }
      if (resolveStringTarget(receiver)) {
        resolvedOut = "/string/" + normalizedMethodName;
        return true;
      }
      if (setIndexedArgsPackMapMethodTarget(receiver, normalizedMethodName)) {
        return true;
      }
    }
    if ((normalizedMethodName == "at" || normalizedMethodName == "at_unsafe") &&
        resolveMapTarget(receiver, keyType, valueType)) {
      resolvedOut = preferredMapMethodTargetForCall(receiver, normalizedMethodName);
      return true;
    }
    if ((normalizedMethodName == "get" || normalizedMethodName == "ref") &&
        resolveSoaVectorTarget(receiver, elemType)) {
      resolvedOut = "/soa_vector/" + normalizedMethodName;
      return true;
    }
  }
  if (receiver.kind == Expr::Kind::Name) {
    if (const BindingInfo *paramBinding = findParamBinding(params, receiver.name)) {
      typeName = paramBinding->typeName;
      typeTemplateArg = paramBinding->typeTemplateArg;
    } else {
      auto it = locals.find(receiver.name);
      if (it != locals.end()) {
        typeName = it->second.typeName;
        typeTemplateArg = it->second.typeTemplateArg;
      }
    }
  }
  if (typeName.empty()) {
    std::string receiverTypeText;
    if (receiver.kind == Expr::Kind::Call &&
        inferQueryExprTypeText(receiver, params, locals, receiverTypeText) &&
        !receiverTypeText.empty()) {
      const std::string normalizedReceiverType = normalizeBindingTypeName(receiverTypeText);
      const std::string receiverCollectionTypePath =
          inferCollectionTypePathFromTypeText(normalizedReceiverType, inferCollectionTypePathFromTypeText);
      if (!receiverCollectionTypePath.empty() &&
          resolveCollectionMethodFromTypePath(receiverCollectionTypePath)) {
        return true;
      }
      typeName = normalizedReceiverType;
    }
  }
  if (typeName.empty()) {
    typeName = inferPointerLikeCallReturnType(receiver);
  }
  if (typeName.empty()) {
    ReturnKind inferredKind = inferExprReturnKind(receiver, params, locals);
    std::string inferred;
    if (inferredKind == ReturnKind::Array) {
      inferred = inferStructReturnPath(receiver, params, locals);
      if (inferred.empty()) {
        inferred = typeNameForReturnKind(inferredKind);
      }
    } else {
      inferred = typeNameForReturnKind(inferredKind);
    }
    if (!inferred.empty()) {
      typeName = inferred;
    }
  }
  if (typeName.empty() && receiver.kind == Expr::Kind::Call && !receiver.isBinding && !receiver.isMethodCall) {
    BindingInfo callBinding;
    std::vector<std::string> resolvedCandidates;
    auto appendResolvedCandidate = [&](const std::string &candidate) {
      if (candidate.empty()) {
        return;
      }
      for (const auto &existing : resolvedCandidates) {
        if (existing == candidate) {
          return;
        }
      }
      resolvedCandidates.push_back(candidate);
    };
    const std::string resolvedReceiverPath = resolveCalleePath(receiver);
    appendResolvedCandidate(resolvedReceiverPath);
    if (resolvedReceiverPath.rfind("/vector/", 0) == 0) {
      appendResolvedCandidate("/std/collections/vector/" +
                              resolvedReceiverPath.substr(std::string("/vector/").size()));
    } else if (resolvedReceiverPath.rfind("/std/collections/vector/", 0) == 0) {
      appendResolvedCandidate("/vector/" +
                              resolvedReceiverPath.substr(std::string("/std/collections/vector/").size()));
    } else if (resolvedReceiverPath.rfind("/map/", 0) == 0) {
      appendResolvedCandidate("/std/collections/map/" +
                              resolvedReceiverPath.substr(std::string("/map/").size()));
    } else if (resolvedReceiverPath.rfind("/std/collections/map/", 0) == 0) {
      appendResolvedCandidate("/map/" +
                              resolvedReceiverPath.substr(std::string("/std/collections/map/").size()));
    }
    for (const auto &candidate : resolvedCandidates) {
      auto defIt = defMap_.find(candidate);
      if (defIt == defMap_.end() || defIt->second == nullptr) {
        continue;
      }
      if (!inferDefinitionReturnBinding(*defIt->second, callBinding)) {
        continue;
      }
      typeName = callBinding.typeName;
      typeTemplateArg = callBinding.typeTemplateArg;
      break;
    }
  }
  if (typeName.empty()) {
    return false;
  }
  if (typeName == "FileError" && (normalizedMethodName == "why" || normalizedMethodName == "is_eof")) {
    if (normalizedMethodName == "why") {
      resolvedOut = defMap_.count("/FileError/why") > 0 ? "/FileError/why" : "/file_error/why";
    } else if (defMap_.count("/FileError/is_eof") > 0) {
      resolvedOut = "/FileError/is_eof";
    } else if (defMap_.count("/std/file/fileErrorIsEof") > 0) {
      resolvedOut = "/std/file/fileErrorIsEof";
    } else {
      return false;
    }
    return true;
  }
  if (typeName == "Pointer" || typeName == "Reference") {
    if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
      resolvedOut = "/" + typeName + "/" + normalizedMethodName;
      return true;
    }
    return false;
  }
  if (isPrimitiveBindingTypeName(typeName)) {
    resolvedOut = "/" + normalizeBindingTypeName(typeName) + "/" + normalizedMethodName;
    return true;
  }
  std::string resolvedType = resolveStructTypePathForMethod(typeName, expr.namespacePrefix);
  if (resolvedType.empty()) {
    resolvedType = resolveTypePath(typeName, expr.namespacePrefix);
  }
  resolvedOut = resolvedType + "/" + normalizedMethodName;
  return true;
}

} // namespace primec::semantics
