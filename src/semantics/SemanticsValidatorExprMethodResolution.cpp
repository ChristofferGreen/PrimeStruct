#include "SemanticsValidator.h"

#include <string>
#include <string_view>

namespace primec::semantics {
namespace {

bool isRemovedVectorCompatibilityHelper(std::string_view helperName) {
  return helperName == "count" || helperName == "capacity" || helperName == "at" ||
         helperName == "at_unsafe" || helperName == "push" || helperName == "pop" ||
         helperName == "reserve" || helperName == "clear" || helperName == "remove_at" ||
         helperName == "remove_swap";
}

bool isRemovedMapCompatibilityHelper(std::string_view helperName) {
  return helperName == "count" || helperName == "at" || helperName == "at_unsafe";
}

bool isFileMethodName(std::string_view methodName) {
  return methodName == "write" || methodName == "write_line" || methodName == "write_byte" ||
         methodName == "read_byte" || methodName == "write_bytes" || methodName == "flush" ||
         methodName == "close";
}

} // namespace

bool SemanticsValidator::isStaticHelperDefinition(const Definition &def) const {
  for (const auto &transform : def.transforms) {
    if (transform.name == "static") {
      return true;
    }
  }
  return false;
}

bool SemanticsValidator::hasDeclaredDefinitionPath(const std::string &path) const {
  for (const auto &def : program_.definitions) {
    if (def.fullPath == path) {
      return true;
    }
  }
  return false;
}

bool SemanticsValidator::resolveMethodTarget(const std::vector<ParameterInfo> &params,
                                             const std::unordered_map<std::string, BindingInfo> &locals,
                                             const std::string &callNamespacePrefix,
                                             const Expr &receiver,
                                             const std::string &methodName,
                                             std::string &resolvedOut,
                                             bool &isBuiltinOut) {
  isBuiltinOut = false;
  auto explicitRemovedCollectionMethodPath = [&](const std::string &rawMethodName) -> std::string {
    std::string candidate = rawMethodName;
    if (!candidate.empty() && candidate.front() == '/') {
      candidate.erase(candidate.begin());
    }
    std::string normalizedPrefix = callNamespacePrefix;
    if (!normalizedPrefix.empty() && normalizedPrefix.front() == '/') {
      normalizedPrefix.erase(normalizedPrefix.begin());
    }
    std::string_view helperName;
    bool isStdNamespacedVectorHelper = false;
    std::string compatibilityCollection;
    if (normalizedPrefix == "array") {
      helperName = candidate;
      compatibilityCollection = "array";
    } else if (normalizedPrefix == "vector") {
      helperName = candidate;
      compatibilityCollection = "vector";
    } else if (normalizedPrefix == "std/collections/vector") {
      helperName = candidate;
      isStdNamespacedVectorHelper = true;
      compatibilityCollection = "vector";
    } else if (normalizedPrefix == "map") {
      helperName = candidate;
      compatibilityCollection = "map";
    } else if (normalizedPrefix == "std/collections/map") {
      helperName = candidate;
      compatibilityCollection = "map";
    } else if (candidate.rfind("array/", 0) == 0) {
      helperName = std::string_view(candidate).substr(std::string_view("array/").size());
      compatibilityCollection = "array";
    } else if (candidate.rfind("vector/", 0) == 0) {
      helperName = std::string_view(candidate).substr(std::string_view("vector/").size());
      compatibilityCollection = "vector";
    } else if (candidate.rfind("std/collections/vector/", 0) == 0) {
      helperName = std::string_view(candidate).substr(std::string_view("std/collections/vector/").size());
      isStdNamespacedVectorHelper = true;
      compatibilityCollection = "vector";
    } else if (candidate.rfind("map/", 0) == 0) {
      helperName = std::string_view(candidate).substr(std::string_view("map/").size());
      compatibilityCollection = "map";
    } else if (candidate.rfind("std/collections/map/", 0) == 0) {
      helperName = std::string_view(candidate).substr(std::string_view("std/collections/map/").size());
      compatibilityCollection = "map";
    }
    if (helperName.empty()) {
      return "";
    }
    if (compatibilityCollection == "map") {
      if (!isRemovedMapCompatibilityHelper(helperName)) {
        return "";
      }
      return "/map/" + std::string(helperName);
    }
    if (!isRemovedVectorCompatibilityHelper(helperName)) {
      return "";
    }
    if (isStdNamespacedVectorHelper) {
      return "/vector/" + std::string(helperName);
    }
    return "/" + candidate;
  };

  const std::string explicitRemovedMethodPath = explicitRemovedCollectionMethodPath(methodName);
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

  auto isStaticBinding = [&](const Expr &bindingExpr) -> bool {
    for (const auto &transform : bindingExpr.transforms) {
      if (transform.name == "static") {
        return true;
      }
    }
    return false;
  };
  auto resolveStructTypePath = [&](const std::string &typeName,
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

  if (normalizedMethodName == "ok" && receiver.kind == Expr::Kind::Name && receiver.name == "Result") {
    resolvedOut = "/result/ok";
    isBuiltinOut = true;
    return true;
  }
  if (normalizedMethodName == "error" && receiver.kind == Expr::Kind::Name && receiver.name == "Result") {
    resolvedOut = "/result/error";
    isBuiltinOut = true;
    return true;
  }
  if (normalizedMethodName == "why" && receiver.kind == Expr::Kind::Name && receiver.name == "Result") {
    resolvedOut = "/result/why";
    isBuiltinOut = true;
    return true;
  }
  if ((normalizedMethodName == "map" || normalizedMethodName == "and_then" || normalizedMethodName == "map2") &&
      receiver.kind == Expr::Kind::Name && receiver.name == "Result") {
    resolvedOut = "/result/" + normalizedMethodName;
    isBuiltinOut = true;
    return true;
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
    const std::string resolvedType = resolveStructTypePath(receiver.name, receiver.namespacePrefix);
    if (!resolvedType.empty()) {
      resolvedOut = resolvedType + "/" + normalizedMethodName;
      return true;
    }
  }

  auto definitionPathContains = [&](std::string_view needle) {
    return currentDefinitionPath_.find(std::string(needle)) != std::string::npos;
  };
  auto preferredExperimentalMapHelperTarget = [&](std::string_view helperName) {
    if (helperName == "count") {
      return std::string("mapCount");
    }
    if (helperName == "contains") {
      return std::string("mapContains");
    }
    if (helperName == "tryAt") {
      return std::string("mapTryAt");
    }
    if (helperName == "at") {
      return std::string("mapAt");
    }
    if (helperName == "at_unsafe") {
      return std::string("mapAtUnsafe");
    }
    return std::string(helperName);
  };
  auto preferredCanonicalExperimentalMapHelperTarget = [&](std::string_view helperName) {
    return "/std/collections/experimental_map/" + preferredExperimentalMapHelperTarget(helperName);
  };
  auto shouldBuiltinValidateCurrentMapWrapperHelper = [&](std::string_view helperName) {
    if (helperName == "count") {
      return definitionPathContains("/mapCount");
    }
    if (helperName == "contains") {
      return definitionPathContains("/mapContains") ||
             definitionPathContains("/mapTryAt");
    }
    if (helperName == "tryAt") {
      return definitionPathContains("/mapTryAt");
    }
    if (helperName == "at") {
      return definitionPathContains("/mapAt");
    }
    if (helperName == "at_unsafe") {
      return definitionPathContains("/mapAtUnsafe");
    }
    return false;
  };

  std::string elemType;
  auto setCollectionMethodTarget = [&](const std::string &path) {
    if (!explicitRemovedMethodPath.empty() && path.rfind("/string/", 0) != 0) {
      resolvedOut = explicitRemovedMethodPath;
      isBuiltinOut = defMap_.count(resolvedOut) == 0;
      return true;
    }
    resolvedOut = preferVectorStdlibHelperPath(path);
    if ((resolvedOut == "/std/collections/map/count" ||
         resolvedOut == "/std/collections/map/contains" ||
         resolvedOut == "/std/collections/map/at" ||
         resolvedOut == "/std/collections/map/at_unsafe") &&
        (shouldBuiltinValidateCurrentMapWrapperHelper(
             resolvedOut.substr(std::string("/std/collections/map/").size())) ||
         hasImportedDefinitionPath(resolvedOut))) {
      isBuiltinOut = true;
      return true;
    }
    isBuiltinOut = defMap_.count(resolvedOut) == 0 &&
                   !hasImportedDefinitionPath(resolvedOut);
    return true;
  };
  auto preferredMapMethodTarget = [&](const Expr &receiverExpr, const std::string &helperName) {
    std::string keyType;
    std::string valueType;
    if (resolveExperimentalMapTarget(receiverExpr, keyType, valueType)) {
      const std::string canonical = "/std/collections/map/" + helperName;
      if (defMap_.count(canonical) > 0 || hasImportedDefinitionPath(canonical)) {
        return canonical;
      }
      return preferredCanonicalExperimentalMapHelperTarget(helperName);
    }
    const std::string canonical = "/std/collections/map/" + helperName;
    const std::string alias = "/map/" + helperName;
    if (defMap_.count(canonical) > 0 || hasImportedDefinitionPath(canonical)) {
      return canonical;
    }
    if (defMap_.count(alias) > 0) {
      return alias;
    }
    return canonical;
  };
  auto resolveArgsPackElementMethodTarget = [&](const std::string &elementTypeText,
                                                const Expr &receiverExpr) -> bool {
    const std::string normalizedElemType = normalizeBindingTypeName(elementTypeText);
    std::string collectionElemType = normalizedElemType;
    std::string wrappedPointeeType;
    if (extractWrappedPointeeType(normalizedElemType, wrappedPointeeType)) {
      collectionElemType = normalizeBindingTypeName(wrappedPointeeType);
    }
    if (collectionElemType == "string") {
      return setCollectionMethodTarget("/string/" + normalizedMethodName);
    }
    if (collectionElemType == "FileError" && normalizedMethodName == "why") {
      resolvedOut = defMap_.count("/FileError/why") > 0 ? "/FileError/why" : "/file_error/why";
      isBuiltinOut = resolvedOut == "/file_error/why";
      return true;
    }
    std::string elemBase;
    std::string elemArgText;
    if (splitTemplateTypeName(collectionElemType, elemBase, elemArgText)) {
      elemBase = normalizeBindingTypeName(elemBase);
      if (elemBase == "vector" || elemBase == "array" || elemBase == "soa_vector") {
        return setCollectionMethodTarget("/" + elemBase + "/" + normalizedMethodName);
      }
      if (elemBase == "map") {
        return setCollectionMethodTarget(preferredMapMethodTarget(receiverExpr, normalizedMethodName));
      }
      if (elemBase == "File" && isFileMethodName(normalizedMethodName)) {
        resolvedOut = "/file/" + normalizedMethodName;
        isBuiltinOut = true;
        return true;
      }
    }
    if (isPrimitiveBindingTypeName(normalizedElemType)) {
      resolvedOut = "/" + normalizedElemType + "/" + normalizedMethodName;
      return true;
    }
    std::string resolvedElemType = resolveStructTypePath(collectionElemType, receiverExpr.namespacePrefix);
    if (resolvedElemType.empty()) {
      resolvedElemType = resolveTypePath(collectionElemType, receiverExpr.namespacePrefix);
    }
    if (!resolvedElemType.empty()) {
      resolvedOut = resolvedElemType + "/" + normalizedMethodName;
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
    const Expr *accessReceiver = resolveBuiltinAccessReceiverExpr(receiverExpr);
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
    resolvedOut = preferredMapMethodTarget(receiverExpr, helperName);
    isBuiltinOut = true;
    return true;
  };
  auto isDirectMapConstructorReceiverCall = [&](const Expr &receiverExpr) {
    if (receiverExpr.kind != Expr::Kind::Call || receiverExpr.isBinding || receiverExpr.isMethodCall) {
      return false;
    }
    const std::string resolvedReceiver = resolveCalleePath(receiverExpr);
    auto matchesPath = [&](std::string_view basePath) {
      return resolvedReceiver == basePath || resolvedReceiver.rfind(std::string(basePath) + "__t", 0) == 0;
    };
    return matchesPath("/std/collections/map/map") ||
           matchesPath("/std/collections/mapNew") ||
           matchesPath("/std/collections/mapSingle") ||
           matchesPath("/std/collections/mapDouble") ||
           matchesPath("/std/collections/mapPair") ||
           matchesPath("/std/collections/mapTriple") ||
           matchesPath("/std/collections/mapQuad") ||
           matchesPath("/std/collections/mapQuint") ||
           matchesPath("/std/collections/mapSext") ||
           matchesPath("/std/collections/mapSept") ||
           matchesPath("/std/collections/mapOct") ||
           matchesPath("/std/collections/experimental_map/mapNew") ||
           matchesPath("/std/collections/experimental_map/mapSingle") ||
           matchesPath("/std/collections/experimental_map/mapDouble") ||
           matchesPath("/std/collections/experimental_map/mapPair") ||
           matchesPath("/std/collections/experimental_map/mapTriple") ||
           matchesPath("/std/collections/experimental_map/mapQuad") ||
           matchesPath("/std/collections/experimental_map/mapQuint") ||
           matchesPath("/std/collections/experimental_map/mapSext") ||
           matchesPath("/std/collections/experimental_map/mapSept") ||
           matchesPath("/std/collections/experimental_map/mapOct");
  };

  if ((normalizedMethodName == "count" || normalizedMethodName == "contains" ||
       normalizedMethodName == "tryAt" || normalizedMethodName == "at" ||
       normalizedMethodName == "at_unsafe") &&
      isDirectMapConstructorReceiverCall(receiver)) {
    return setCollectionMethodTarget(preferredMapMethodTarget(receiver, normalizedMethodName));
  }
  if (normalizedMethodName == "count") {
    if (resolveArgsPackCountTarget(receiver, elemType)) {
      return setCollectionMethodTarget("/array/count");
    }
    if (resolveVectorTarget(receiver, elemType)) {
      return setCollectionMethodTarget("/vector/count");
    }
    if (resolveSoaVectorTarget(receiver, elemType)) {
      return setCollectionMethodTarget("/soa_vector/count");
    }
    if (resolveArrayTarget(receiver, elemType)) {
      return setCollectionMethodTarget("/array/count");
    }
    if (resolveStringTarget(receiver)) {
      return setCollectionMethodTarget("/string/count");
    }
    if (setIndexedArgsPackMapMethodTarget(receiver, "count")) {
      return true;
    }
    if (resolveMapTarget(receiver)) {
      return setCollectionMethodTarget(preferredMapMethodTarget(receiver, "count"));
    }
  }
  if (normalizedMethodName == "contains" || normalizedMethodName == "tryAt") {
    if (setIndexedArgsPackMapMethodTarget(receiver, normalizedMethodName)) {
      return true;
    }
    if (resolveMapTarget(receiver)) {
      return setCollectionMethodTarget(preferredMapMethodTarget(receiver, normalizedMethodName));
    }
  }
  if (normalizedMethodName == "capacity") {
    if (resolveVectorTarget(receiver, elemType)) {
      return setCollectionMethodTarget("/vector/capacity");
    }
  }
  if (normalizedMethodName == "at" || normalizedMethodName == "at_unsafe") {
    if (resolveArgsPackAccessTarget(receiver, elemType)) {
      return setCollectionMethodTarget("/array/" + normalizedMethodName);
    }
    if (resolveVectorTarget(receiver, elemType)) {
      return setCollectionMethodTarget("/vector/" + normalizedMethodName);
    }
    if (resolveArrayTarget(receiver, elemType)) {
      return setCollectionMethodTarget("/array/" + normalizedMethodName);
    }
    if (resolveStringTarget(receiver)) {
      return setCollectionMethodTarget("/string/" + normalizedMethodName);
    }
    if (setIndexedArgsPackMapMethodTarget(receiver, normalizedMethodName)) {
      return true;
    }
    if (resolveMapTarget(receiver)) {
      return setCollectionMethodTarget(preferredMapMethodTarget(receiver, normalizedMethodName));
    }
  }
  if (normalizedMethodName == "get" || normalizedMethodName == "ref") {
    if (resolveSoaVectorTarget(receiver, elemType)) {
      return setCollectionMethodTarget("/soa_vector/" + normalizedMethodName);
    }
  }
  if (resolveSoaVectorTarget(receiver, elemType)) {
    const std::string normalizedElemType = normalizeBindingTypeName(elemType);
    std::string currentNamespace;
    if (!currentDefinitionPath_.empty()) {
      const size_t slash = currentDefinitionPath_.find_last_of('/');
      if (slash != std::string::npos && slash > 0) {
        currentNamespace = currentDefinitionPath_.substr(0, slash);
      }
    }
    const std::string lookupNamespace =
        !receiver.namespacePrefix.empty() ? receiver.namespacePrefix : currentNamespace;
    const std::string elementStructPath = resolveStructTypePath(normalizedElemType, lookupNamespace);
    if (!elementStructPath.empty()) {
      auto structIt = defMap_.find(elementStructPath);
      if (structIt != defMap_.end() && structIt->second != nullptr) {
        for (const auto &stmt : structIt->second->statements) {
          if (!stmt.isBinding || isStaticBinding(stmt) || stmt.name != normalizedMethodName) {
            continue;
          }
          const std::string helperPath = "/soa_vector/" + normalizedMethodName;
          if (defMap_.count(helperPath) > 0) {
            resolvedOut = helperPath;
            isBuiltinOut = false;
          } else {
            resolvedOut = "/soa_vector/field_view/" + normalizedMethodName;
            isBuiltinOut = true;
          }
          return true;
        }
      }
    }
  }
  if (receiver.kind == Expr::Kind::Call && !receiver.isBinding) {
    std::string accessHelperName;
    if (getBuiltinArrayAccessName(receiver, accessHelperName) && !receiver.args.empty()) {
      const std::string removedMapCompatibilityPath = getDirectMapHelperCompatibilityPath(receiver);
      if (!removedMapCompatibilityPath.empty()) {
        error_ = "unknown call target: " + removedMapCompatibilityPath;
        return false;
      }
      size_t accessReceiverIndex = 0;
      if (!receiver.isMethodCall && hasNamedArguments(receiver.argNames)) {
        bool foundValues = false;
        for (size_t i = 0; i < receiver.args.size(); ++i) {
          if (i < receiver.argNames.size() && receiver.argNames[i].has_value() &&
              *receiver.argNames[i] == "values") {
            accessReceiverIndex = i;
            foundValues = true;
            break;
          }
        }
        if (!foundValues) {
          accessReceiverIndex = 0;
        }
      }
      if (accessReceiverIndex < receiver.args.size()) {
        const Expr &accessReceiver = receiver.args[accessReceiverIndex];
        std::string accessElemType;
        std::string accessValueType;
        if (resolveArgsPackAccessTarget(accessReceiver, accessElemType) ||
            resolveVectorTarget(accessReceiver, accessElemType) ||
            resolveArrayTarget(accessReceiver, accessElemType)) {
          const std::string normalizedElemType =
              normalizeBindingTypeName(unwrapReferencePointerTypeText(accessElemType));
          if (normalizedElemType == "string") {
            return setCollectionMethodTarget("/string/" + normalizedMethodName);
          }
          std::string elemBase;
          std::string elemArgText;
          if (splitTemplateTypeName(normalizedElemType, elemBase, elemArgText)) {
            elemBase = normalizeBindingTypeName(elemBase);
            if (elemBase == "vector" || elemBase == "array" || elemBase == "soa_vector") {
              return setCollectionMethodTarget("/" + elemBase + "/" + normalizedMethodName);
            }
            if (elemBase == "map") {
              if (setIndexedArgsPackMapMethodTarget(receiver, normalizedMethodName)) {
                return true;
              }
              return setCollectionMethodTarget(preferredMapMethodTarget(receiver, normalizedMethodName));
            }
            if (elemBase == "File" && isFileMethodName(normalizedMethodName)) {
              resolvedOut = "/file/" + normalizedMethodName;
              isBuiltinOut = true;
              return true;
            }
          }
          if (normalizedElemType == "FileError" && normalizedMethodName == "why") {
            resolvedOut = defMap_.count("/FileError/why") > 0 ? "/FileError/why" : "/file_error/why";
            isBuiltinOut = resolvedOut == "/file_error/why";
            return true;
          }
          if (isPrimitiveBindingTypeName(normalizedElemType)) {
            resolvedOut = "/" + normalizedElemType + "/" + normalizedMethodName;
            return true;
          }
          std::string resolvedElemType = resolveStructTypePath(normalizedElemType, receiver.namespacePrefix);
          if (resolvedElemType.empty()) {
            resolvedElemType = resolveTypePath(normalizedElemType, receiver.namespacePrefix);
          }
          if (!resolvedElemType.empty()) {
            resolvedOut = resolvedElemType + "/" + normalizedMethodName;
            return true;
          }
        }
        if (resolveStringTarget(accessReceiver)) {
          resolvedOut = "/i32/" + normalizedMethodName;
          return true;
        }
        if (resolveMapValueType(accessReceiver, accessValueType) &&
            resolveCalleePath(receiver).rfind("/map/", 0) == 0) {
          const std::string normalizedValueType = normalizeBindingTypeName(accessValueType);
          if (isPrimitiveBindingTypeName(normalizedValueType)) {
            resolvedOut = "/" + normalizedValueType + "/" + normalizedMethodName;
            return true;
          }
          std::string resolvedValueType = resolveStructTypePath(normalizedValueType, receiver.namespacePrefix);
          if (resolvedValueType.empty()) {
            resolvedValueType = resolveTypePath(normalizedValueType, receiver.namespacePrefix);
          }
          if (!resolvedValueType.empty()) {
            resolvedOut = resolvedValueType + "/" + normalizedMethodName;
            return true;
          }
        }
      }
    }
  }
  if (receiver.kind == Expr::Kind::Call && !receiver.isBinding) {
    std::string dereferencedElemType;
    if (resolveDereferencedIndexedArgsPackElementType(receiver, dereferencedElemType) &&
        resolveArgsPackElementMethodTarget(dereferencedElemType, receiver)) {
      return true;
    }
  }
  if (receiver.kind == Expr::Kind::Call && !receiver.isBinding && !receiver.isMethodCall) {
    const std::string resolvedType = resolveCalleePath(receiver);
    if (!resolvedType.empty() && structNames_.count(resolvedType) > 0) {
      resolvedOut = resolvedType + "/" + normalizedMethodName;
      return true;
    }
  }

  std::string typeName;
  if (receiver.kind == Expr::Kind::Name) {
    if (const BindingInfo *paramBinding = findParamBinding(params, receiver.name)) {
      typeName = paramBinding->typeName;
    } else {
      auto it = locals.find(receiver.name);
      if (it != locals.end()) {
        typeName = it->second.typeName;
      }
    }
  }
  if (typeName.empty()) {
    std::string inferredStruct = inferStructReturnPath(receiver, params, locals);
    if (!inferredStruct.empty()) {
      std::string normalizedStruct = normalizeBindingTypeName(inferredStruct);
      if (!normalizedStruct.empty() && normalizedStruct.front() != '/') {
        normalizedStruct.insert(normalizedStruct.begin(), '/');
      }
      if (normalizedStruct == "/map" ||
          normalizedStruct.rfind("/std/collections/experimental_map/Map__", 0) == 0) {
        typeName = "/map";
      } else {
        typeName = inferredStruct;
      }
    }
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
  if (typeName == "File" && isFileMethodName(normalizedMethodName)) {
    resolvedOut = "/file/" + normalizedMethodName;
    isBuiltinOut = true;
    return true;
  }
  if (typeName == "FileError" && normalizedMethodName == "why") {
    resolvedOut = defMap_.count("/FileError/why") > 0 ? "/FileError/why" : "/file_error/why";
    isBuiltinOut = resolvedOut == "/file_error/why";
    return true;
  }
  if (typeName == "string" &&
      (normalizedMethodName == "count" || normalizedMethodName == "at" || normalizedMethodName == "at_unsafe")) {
    return setCollectionMethodTarget("/string/" + normalizedMethodName);
  }
  if (typeName.empty()) {
    if (receiver.kind == Expr::Kind::Call && !validateExpr(params, locals, receiver)) {
      return false;
    }
    error_ = "unknown method target for " + normalizedMethodName;
    return false;
  }
  if (typeName == "Pointer" || typeName == "Reference") {
    error_ = "unknown method target for " + normalizedMethodName;
    return false;
  }
  if (isPrimitiveBindingTypeName(typeName)) {
    resolvedOut = "/" + normalizeBindingTypeName(typeName) + "/" + normalizedMethodName;
    return true;
  }
  std::string resolvedType = resolveStructTypePath(typeName, receiver.namespacePrefix);
  if (resolvedType.empty()) {
    resolvedType = resolveTypePath(typeName, receiver.namespacePrefix);
  }
  resolvedOut = resolvedType + "/" + normalizedMethodName;
  return true;
}

} // namespace primec::semantics
