#include "IrLowererSetupTypeHelpers.h"

#include "IrLowererCallHelpers.h"
#include "IrLowererHelpers.h"
#include "IrLowererSetupTypeCollectionHelpers.h"
#include "IrLowererSetupTypeReceiverTargetHelpers.h"
#include "IrLowererTemplateTypeParseHelpers.h"

namespace primec::ir_lowerer {

namespace {

std::string resolveScopedMethodPath(const Expr &expr) {
  if (!expr.name.empty() && expr.name.front() == '/') {
    return expr.name;
  }
  if (!expr.namespacePrefix.empty()) {
    std::string scoped = expr.namespacePrefix;
    if (!scoped.empty() && scoped.front() != '/') {
      scoped.insert(scoped.begin(), '/');
    }
    return scoped + "/" + expr.name;
  }
  return expr.name;
}

std::string resolveStructTypePathFromName(const std::string &typeName,
                                          const std::string &namespacePrefix,
                                          const std::unordered_map<std::string, std::string> &importAliases,
                                          const std::unordered_set<std::string> &structNames) {
  if (typeName.empty()) {
    return "";
  }
  if (typeName.front() == '/') {
    return structNames.count(typeName) > 0 ? typeName : "";
  }
  std::string current = namespacePrefix;
  while (true) {
    if (!current.empty()) {
      const std::string scoped = current + "/" + typeName;
      if (structNames.count(scoped) > 0) {
        return scoped;
      }
      if (current.size() > typeName.size()) {
        const size_t start = current.size() - typeName.size();
        if (start > 0 && current[start - 1] == '/' &&
            current.compare(start, typeName.size(), typeName) == 0 &&
            structNames.count(current) > 0) {
          return current;
        }
      }
    } else {
      const std::string root = "/" + typeName;
      if (structNames.count(root) > 0) {
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
  auto importIt = importAliases.find(typeName);
  if (importIt != importAliases.end()) {
    return importIt->second;
  }
  return "";
}

} // namespace

bool inferReceiverTypeFromDeclaredReturn(const Definition &definition, std::string &typeNameOut) {
  typeNameOut.clear();

  std::string collectionName;
  std::vector<std::string> collectionArgs;
  if (inferDeclaredReturnCollection(definition, collectionName, collectionArgs)) {
    typeNameOut = collectionName;
    return true;
  }

  auto unwrapDeclaredReturnType = [](std::string typeText) {
    while (!typeText.empty()) {
      std::string base;
      std::string argText;
      if (!splitTemplateTypeName(typeText, base, argText)) {
        break;
      }
      base = normalizeDeclaredCollectionTypeBase(trimTemplateTypeText(base));
      std::vector<std::string> args;
      if ((base != "Reference" && base != "Pointer") || !splitTemplateArgs(argText, args) || args.size() != 1) {
        break;
      }
      typeText = trimTemplateTypeText(args.front());
    }
    return typeText;
  };

  for (const auto &transform : definition.transforms) {
    if (transform.name != "return" || transform.templateArgs.size() != 1) {
      continue;
    }
    std::string declaredType = trimTemplateTypeText(transform.templateArgs.front());
    if (declaredType == "auto") {
      break;
    }
    declaredType = unwrapDeclaredReturnType(std::move(declaredType));

    std::string base;
    std::string argText;
    if (splitTemplateTypeName(declaredType, base, argText)) {
      base = normalizeDeclaredCollectionTypeBase(trimTemplateTypeText(base));
      if (!base.empty() && base.front() == '/') {
        typeNameOut = base;
        return true;
      }
      typeNameOut = base;
      return !typeNameOut.empty();
    }

    declaredType = normalizeDeclaredCollectionTypeBase(trimTemplateTypeText(declaredType));
    if (declaredType.empty()) {
      return false;
    }
    if (declaredType.front() == '/') {
      typeNameOut = declaredType;
      return true;
    }
    typeNameOut = declaredType;
    return true;
  }

  return false;
}

bool resolveMethodCallReceiverExpr(const Expr &callExpr,
                                   const LocalMap &localsIn,
                                   const IsMethodCallClassifierFn &isArrayCountCall,
                                   const IsMethodCallClassifierFn &isVectorCapacityCall,
                                   const IsMethodCallClassifierFn &isEntryArgsName,
                                   const Expr *&receiverOut,
                                   std::string &errorOut) {
  receiverOut = nullptr;

  if (callExpr.kind != Expr::Kind::Call || callExpr.isBinding || !callExpr.isMethodCall) {
    return false;
  }
  if (callExpr.args.empty()) {
    errorOut = "method call missing receiver";
    return false;
  }
  std::string accessName;
  const bool isBuiltinAccessCall = getBuiltinArrayAccessName(callExpr, accessName) && callExpr.args.size() == 2;
  const bool isBuiltinCountOrCapacityCall =
      isVectorBuiltinName(callExpr, "count") || isMapBuiltinName(callExpr, "count") ||
      isVectorBuiltinName(callExpr, "capacity");
  const bool isBuiltinBareVectorCapacityMethod =
      isSimpleCallName(callExpr, "capacity") &&
      isVectorCapacityCall && isVectorCapacityCall(callExpr, localsIn);
  const bool isBuiltinVectorMutatorCall =
      isVectorBuiltinName(callExpr, "push") || isVectorBuiltinName(callExpr, "pop") ||
      isVectorBuiltinName(callExpr, "reserve") || isVectorBuiltinName(callExpr, "clear") ||
      isVectorBuiltinName(callExpr, "remove_at") || isVectorBuiltinName(callExpr, "remove_swap");
  const std::string scopedMethodPath = resolveScopedMethodPath(callExpr);
  const bool isExplicitRemovedVectorMethodAlias = isExplicitRemovedVectorMethodAliasPath(scopedMethodPath);
  const bool isExplicitMapMethodAlias = isExplicitMapMethodAliasPath(scopedMethodPath);
  const bool isExplicitMapContainsOrTryAtMethod =
      isExplicitMapContainsOrTryAtMethodPath(scopedMethodPath);
  const bool isBuiltinMapContainsOrTryAtCall =
      isSimpleCallName(callExpr, "contains") || isSimpleCallName(callExpr, "tryAt") ||
      isSimpleCallName(callExpr, "insert");
  const bool allowBuiltinFallback =
      !isExplicitRemovedVectorMethodAlias && !isExplicitMapMethodAlias &&
      !isExplicitMapContainsOrTryAtMethod &&
      !isBuiltinBareVectorCapacityMethod &&
      (isBuiltinCountOrCapacityCall || isBuiltinVectorMutatorCall ||
       isBuiltinMapContainsOrTryAtCall ||
       (isArrayCountCall && isArrayCountCall(callExpr, localsIn)) ||
       (isVectorCapacityCall && isVectorCapacityCall(callExpr, localsIn)) || isBuiltinAccessCall);
  const Expr &receiver = callExpr.args.front();
  if (isEntryArgsName && isEntryArgsName(receiver, localsIn)) {
    if (allowBuiltinFallback) {
      return false;
    }
    errorOut = "unknown method target for " + scopedMethodPath;
    return false;
  }

  receiverOut = &receiver;
  return true;
}

bool resolveMethodReceiverTypeFromLocalInfo(const LocalInfo &localInfo,
                                            std::string &typeNameOut,
                                            std::string &resolvedTypePathOut) {
  typeNameOut.clear();
  resolvedTypePathOut.clear();

  if (localInfo.isFileHandle) {
    typeNameOut = "File";
    return true;
  }
  if (!localInfo.structTypeName.empty()) {
    resolvedTypePathOut = localInfo.structTypeName;
    return true;
  }
  if (!localInfo.errorHelperNamespacePath.empty()) {
    resolvedTypePathOut = localInfo.errorHelperNamespacePath;
    return true;
  }
  if (!localInfo.errorTypeName.empty()) {
    typeNameOut = localInfo.errorTypeName;
    return true;
  }

  if (localInfo.kind == LocalInfo::Kind::Array) {
    typeNameOut = "array";
    return true;
  }
  if (localInfo.isSoaVector) {
    typeNameOut = "soa_vector";
    return true;
  }
  if (localInfo.kind == LocalInfo::Kind::Vector) {
    typeNameOut = "vector";
    return true;
  }
  if (localInfo.kind == LocalInfo::Kind::Map) {
    typeNameOut = "map";
    return true;
  }
  if (localInfo.kind == LocalInfo::Kind::Buffer) {
    typeNameOut = "Buffer";
    return true;
  }
  if (localInfo.kind == LocalInfo::Kind::Reference &&
      (localInfo.referenceToArray || localInfo.referenceToVector || localInfo.referenceToMap ||
       localInfo.referenceToBuffer)) {
    if (!localInfo.structTypeName.empty()) {
      resolvedTypePathOut = localInfo.structTypeName;
    } else {
      typeNameOut = localInfo.referenceToMap ? "map"
                                             : (localInfo.referenceToVector ? (localInfo.isSoaVector ? "soa_vector"
                                                                                                     : "vector")
                                                                            : (localInfo.referenceToBuffer ? "Buffer"
                                                                                                            : "array"));
    }
    return true;
  }
  if (localInfo.kind == LocalInfo::Kind::Pointer && localInfo.pointerToArray) {
    typeNameOut = "array";
    return true;
  }
  if (localInfo.kind == LocalInfo::Kind::Pointer && localInfo.pointerToVector) {
    typeNameOut = localInfo.isSoaVector ? "soa_vector" : "vector";
    return true;
  }
  if (localInfo.kind == LocalInfo::Kind::Pointer && localInfo.pointerToMap) {
    typeNameOut = "map";
    return true;
  }
  if (localInfo.kind == LocalInfo::Kind::Pointer && localInfo.pointerToBuffer) {
    typeNameOut = "Buffer";
    return true;
  }
  if (localInfo.kind == LocalInfo::Kind::Reference && !localInfo.structTypeName.empty()) {
    resolvedTypePathOut = localInfo.structTypeName;
    return true;
  }
  if (localInfo.kind == LocalInfo::Kind::Pointer || localInfo.kind == LocalInfo::Kind::Reference) {
    return false;
  }
  if (localInfo.kind == LocalInfo::Kind::Value && !localInfo.structTypeName.empty()) {
    resolvedTypePathOut = localInfo.structTypeName;
    return true;
  }

  typeNameOut = typeNameForValueKind(localInfo.valueKind);
  return true;
}

std::string resolveMethodReceiverTypeNameFromCallExpr(const Expr &receiverCallExpr,
                                                      LocalInfo::ValueKind inferredKind,
                                                      const ResolveReceiverExprPathFn &resolveExprPath) {
  auto isBufferConstructorCall = [&](const Expr &candidate) {
    const std::string scopedName = resolveExprPath ? resolveExprPath(candidate) : std::string();
    return scopedName == "Buffer" || scopedName == "/std/gfx/Buffer" ||
           scopedName == "/std/gfx/experimental/Buffer" ||
           scopedName.rfind("/std/gfx/Buffer__t", 0) == 0 ||
           scopedName.rfind("/std/gfx/experimental/Buffer__t", 0) == 0;
  };
  if (receiverCallExpr.kind == Expr::Kind::Call && receiverCallExpr.templateArgs.size() == 1 &&
      isBufferConstructorCall(receiverCallExpr)) {
    return "Buffer";
  }
  std::string collection;
  if (getBuiltinCollectionName(receiverCallExpr, collection)) {
    if (collection == "array" && receiverCallExpr.templateArgs.size() == 1) {
      return "array";
    }
    if (collection == "vector" && receiverCallExpr.templateArgs.size() == 1) {
      return "vector";
    }
    if (collection == "map" && receiverCallExpr.templateArgs.size() == 2) {
      return "map";
    }
    if (collection == "Buffer" && receiverCallExpr.templateArgs.size() == 1) {
      return "Buffer";
    }
    if (collection == "soa_vector" && receiverCallExpr.templateArgs.size() == 1) {
      return "soa_vector";
    }
  }
  return typeNameForValueKind(inferredKind);
}

bool inferBuiltinAccessReceiverResultKind(const Expr &receiverCallExpr,
                                          const LocalMap &localsIn,
                                          const InferReceiverExprKindFn &inferExprKind,
                                          const ResolveReceiverExprPathFn &resolveExprPath,
                                          const GetReturnInfoForPathFn &getReturnInfo,
                                          const std::unordered_map<std::string, const Definition *> &defMap,
                                          LocalInfo::ValueKind &kindOut) {
  kindOut = LocalInfo::ValueKind::Unknown;
  const std::string scopedReceiverMethodPath = resolveScopedMethodPath(receiverCallExpr);
  if ((receiverCallExpr.isMethodCall &&
       (isExplicitMapMethodAliasPath(scopedReceiverMethodPath) ||
        isExplicitMapContainsOrTryAtMethodPath(scopedReceiverMethodPath))) ||
      isExplicitMapHelperFallbackPath(receiverCallExpr) ||
      isExplicitVectorReceiverProbeHelperExpr(receiverCallExpr)) {
    return false;
  }

  std::string accessName;
  if (!(receiverCallExpr.kind == Expr::Kind::Call && getBuiltinArrayAccessName(receiverCallExpr, accessName) &&
        receiverCallExpr.args.size() == 2)) {
    return false;
  }

  const Expr &accessReceiver = receiverCallExpr.args.front();
  auto assignKind = [&](LocalInfo::ValueKind valueKind) {
    if (valueKind == LocalInfo::ValueKind::Unknown) {
      return false;
    }
    kindOut = valueKind;
    return true;
  };
  auto assignCollectionKind = [&](const std::string &collectionName, const std::vector<std::string> &collectionArgs) {
    if ((collectionName == "vector" || collectionName == "array") && collectionArgs.size() == 1) {
      return assignKind(valueKindFromTypeName(collectionArgs.front()));
    }
    if (collectionName == "map" && collectionArgs.size() == 2) {
      return assignKind(valueKindFromTypeName(collectionArgs.back()));
    }
    return false;
  };

  if (accessReceiver.kind == Expr::Kind::Name) {
    auto localIt = localsIn.find(accessReceiver.name);
    if (localIt == localsIn.end()) {
      return false;
    }
    const LocalInfo &receiverInfo = localIt->second;
    if (receiverInfo.kind == LocalInfo::Kind::Vector || receiverInfo.kind == LocalInfo::Kind::Array ||
        (receiverInfo.kind == LocalInfo::Kind::Reference &&
         (receiverInfo.referenceToArray || receiverInfo.referenceToVector)) ||
        (receiverInfo.kind == LocalInfo::Kind::Pointer && receiverInfo.pointerToArray) ||
        (receiverInfo.kind == LocalInfo::Kind::Pointer && receiverInfo.pointerToVector)) {
      return assignKind(receiverInfo.valueKind);
    }
    if (receiverInfo.kind == LocalInfo::Kind::Map ||
        (receiverInfo.kind == LocalInfo::Kind::Reference && receiverInfo.referenceToMap) ||
        (receiverInfo.kind == LocalInfo::Kind::Pointer && receiverInfo.pointerToMap)) {
      return assignKind(receiverInfo.mapValueKind);
    }
    if (receiverInfo.kind == LocalInfo::Kind::Value && receiverInfo.valueKind == LocalInfo::ValueKind::String) {
      return assignKind(LocalInfo::ValueKind::Int32);
    }
    return false;
  }

  if (accessReceiver.kind != Expr::Kind::Call) {
    return false;
  }

  std::string collectionName;
  if (getBuiltinCollectionName(accessReceiver, collectionName)) {
    if (collectionName == "string") {
      return assignKind(LocalInfo::ValueKind::Int32);
    }
    if (assignCollectionKind(collectionName, accessReceiver.templateArgs)) {
      return true;
    }
  }

  if (inferExprKind && inferExprKind(accessReceiver, localsIn) == LocalInfo::ValueKind::String) {
    return assignKind(LocalInfo::ValueKind::Int32);
  }

  const std::string receiverPath = resolveExprPath ? resolveExprPath(accessReceiver) : std::string();
  if (receiverPath.empty()) {
    return false;
  }

  auto defIt = defMap.find(receiverPath);
  if (defIt != defMap.end() && defIt->second != nullptr) {
    std::string declaredCollectionName;
    std::vector<std::string> declaredCollectionArgs;
    if (inferDeclaredReturnCollection(*defIt->second, declaredCollectionName, declaredCollectionArgs) &&
        assignCollectionKind(declaredCollectionName, declaredCollectionArgs)) {
      return true;
    }
  }

  ReturnInfo receiverInfo;
  if (getReturnInfo && getReturnInfo(receiverPath, receiverInfo) && !receiverInfo.returnsVoid &&
      !receiverInfo.returnsArray) {
    if (receiverInfo.kind == LocalInfo::ValueKind::String) {
      return assignKind(LocalInfo::ValueKind::Int32);
    }
    return assignKind(receiverInfo.kind);
  }

  return false;
}

bool isSoaVectorReceiverExpr(const Expr &receiverExpr, const LocalMap &localsIn) {
  if (receiverExpr.kind == Expr::Kind::Name) {
    auto it = localsIn.find(receiverExpr.name);
    return it != localsIn.end() && it->second.isSoaVector;
  }
  if (receiverExpr.kind == Expr::Kind::Call) {
    std::string collection;
    return getBuiltinCollectionName(receiverExpr, collection) && collection == "soa_vector";
  }
  return false;
}

bool resolveMethodReceiverTypeFromNameExpr(const Expr &receiverNameExpr,
                                           const LocalMap &localsIn,
                                           const std::string &methodName,
                                           std::string &typeNameOut,
                                           std::string &resolvedTypePathOut,
                                           std::string &errorOut) {
  if (receiverNameExpr.kind != Expr::Kind::Name) {
    errorOut = "internal method receiver type resolution requires name expression";
    return false;
  }

  auto it = localsIn.find(receiverNameExpr.name);
  if (it == localsIn.end()) {
    if (receiverNameExpr.name == "FileError") {
      typeNameOut = "FileError";
      resolvedTypePathOut = "/std/file/FileError";
      return true;
    }
    if (receiverNameExpr.name == "ImageError") {
      typeNameOut = "ImageError";
      resolvedTypePathOut = "/std/image/ImageError";
      return true;
    }
    if (receiverNameExpr.name == "ContainerError") {
      typeNameOut = "ContainerError";
      resolvedTypePathOut = "/std/collections/ContainerError";
      return true;
    }
    if (receiverNameExpr.name == "GfxError") {
      typeNameOut = "GfxError";
      return true;
    }
    errorOut = "native backend does not know identifier: " + receiverNameExpr.name;
    return false;
  }
  if (!resolveMethodReceiverTypeFromLocalInfo(it->second, typeNameOut, resolvedTypePathOut)) {
    errorOut = "unknown method target for " + methodName;
    return false;
  }
  return true;
}

bool resolveMethodReceiverTarget(const Expr &receiverExpr,
                                 const LocalMap &localsIn,
                                 const std::string &methodName,
                                 const std::unordered_map<std::string, std::string> &importAliases,
                                 const std::unordered_set<std::string> &structNames,
                                 const InferReceiverExprKindFn &inferExprKind,
                                 const ResolveReceiverExprPathFn &resolveExprPath,
                                 std::string &typeNameOut,
                                 std::string &resolvedTypePathOut,
                                 std::string &errorOut) {
  typeNameOut.clear();
  resolvedTypePathOut.clear();

  if (receiverExpr.kind == Expr::Kind::Name) {
    if (resolveMethodReceiverTypeFromNameExpr(
            receiverExpr, localsIn, methodName, typeNameOut, resolvedTypePathOut, errorOut)) {
      return true;
    }
    const std::string resolvedStructType =
        resolveStructTypePathFromName(receiverExpr.name, receiverExpr.namespacePrefix, importAliases, structNames);
    if (!resolvedStructType.empty()) {
      errorOut.clear();
      resolvedTypePathOut = resolvedStructType;
      return true;
    }
    return false;
  }
  if (receiverExpr.kind == Expr::Kind::Call) {
    auto resolveDereferencedCollectionOrFileErrorReceiver = [&](const Expr &targetExpr) {
      auto classifyLocal = [&](const LocalInfo &localInfo, bool fromArgsPack) -> bool {
        const LocalInfo::Kind receiverKind =
            fromArgsPack ? localInfo.argsPackElementKind : localInfo.kind;
        const bool isReferenceArray = receiverKind == LocalInfo::Kind::Reference && localInfo.referenceToArray;
        const bool isPointerArray = receiverKind == LocalInfo::Kind::Pointer && localInfo.pointerToArray;
        const bool isReferenceVector = receiverKind == LocalInfo::Kind::Reference && localInfo.referenceToVector;
        const bool isPointerVector = receiverKind == LocalInfo::Kind::Pointer && localInfo.pointerToVector;
        const bool isReferenceMap = receiverKind == LocalInfo::Kind::Reference && localInfo.referenceToMap;
        const bool isPointerMap = receiverKind == LocalInfo::Kind::Pointer && localInfo.pointerToMap;
        const bool isReferenceBuffer = receiverKind == LocalInfo::Kind::Reference && localInfo.referenceToBuffer;
        const bool isPointerBuffer = receiverKind == LocalInfo::Kind::Pointer && localInfo.pointerToBuffer;
        if (localInfo.isFileError &&
            (receiverKind == LocalInfo::Kind::Reference || receiverKind == LocalInfo::Kind::Pointer)) {
          typeNameOut = "FileError";
          return true;
        }
        if (localInfo.isFileHandle &&
            (receiverKind == LocalInfo::Kind::Reference || receiverKind == LocalInfo::Kind::Pointer)) {
          typeNameOut = "File";
          return true;
        }
        if (isReferenceArray || isPointerArray) {
          typeNameOut = "array";
          return true;
        }
        if (isReferenceVector || isPointerVector) {
          typeNameOut = localInfo.isSoaVector ? "soa_vector" : "vector";
          return true;
        }
        if (isReferenceMap || isPointerMap) {
          typeNameOut = "map";
          return true;
        }
        if (isReferenceBuffer || isPointerBuffer) {
          typeNameOut = "Buffer";
          return true;
        }
        return false;
      };

      if (targetExpr.kind == Expr::Kind::Name) {
        auto localIt = localsIn.find(targetExpr.name);
        return localIt != localsIn.end() && classifyLocal(localIt->second, false);
      }

      std::string derefAccessName;
      if (targetExpr.kind == Expr::Kind::Call && getBuiltinArrayAccessName(targetExpr, derefAccessName) &&
          targetExpr.args.size() == 2 && targetExpr.args.front().kind == Expr::Kind::Name) {
        auto localIt = localsIn.find(targetExpr.args.front().name);
        return localIt != localsIn.end() && localIt->second.isArgsPack &&
               classifyLocal(localIt->second, true);
      }

      return false;
    };

    std::string accessName;
    if (getBuiltinArrayAccessName(receiverExpr, accessName) && receiverExpr.args.size() == 2) {
      const Expr &accessReceiver = receiverExpr.args.front();
      if (accessReceiver.kind == Expr::Kind::Name) {
        auto localIt = localsIn.find(accessReceiver.name);
        if (localIt != localsIn.end() && localIt->second.isArgsPack) {
          if (localIt->second.isFileError &&
              (localIt->second.argsPackElementKind == LocalInfo::Kind::Value ||
               localIt->second.argsPackElementKind == LocalInfo::Kind::Reference ||
               localIt->second.argsPackElementKind == LocalInfo::Kind::Pointer)) {
            typeNameOut = "FileError";
            return true;
          }
          if (localIt->second.isFileHandle &&
              (localIt->second.argsPackElementKind == LocalInfo::Kind::Value ||
               localIt->second.argsPackElementKind == LocalInfo::Kind::Reference ||
               localIt->second.argsPackElementKind == LocalInfo::Kind::Pointer)) {
            typeNameOut = "File";
            return true;
          }
          if (!localIt->second.structTypeName.empty()) {
            resolvedTypePathOut = localIt->second.structTypeName;
            return true;
          }
          if (localIt->second.argsPackElementKind == LocalInfo::Kind::Vector) {
            typeNameOut = localIt->second.isSoaVector ? "soa_vector" : "vector";
            return true;
          }
          if (localIt->second.argsPackElementKind == LocalInfo::Kind::Reference &&
              localIt->second.referenceToArray) {
            typeNameOut = "array";
            return true;
          }
          if (localIt->second.argsPackElementKind == LocalInfo::Kind::Pointer &&
              localIt->second.pointerToArray) {
            typeNameOut = "array";
            return true;
          }
          if (localIt->second.argsPackElementKind == LocalInfo::Kind::Reference &&
              localIt->second.referenceToVector) {
            typeNameOut = localIt->second.isSoaVector ? "soa_vector" : "vector";
            return true;
          }
          if (localIt->second.argsPackElementKind == LocalInfo::Kind::Pointer &&
              localIt->second.pointerToVector) {
            typeNameOut = localIt->second.isSoaVector ? "soa_vector" : "vector";
            return true;
          }
          if (localIt->second.argsPackElementKind == LocalInfo::Kind::Reference &&
              localIt->second.referenceToMap) {
            typeNameOut = "map";
            return true;
          }
          if (localIt->second.argsPackElementKind == LocalInfo::Kind::Pointer &&
              localIt->second.pointerToMap) {
            typeNameOut = "map";
            return true;
          }
          if (localIt->second.argsPackElementKind == LocalInfo::Kind::Array) {
            typeNameOut = "array";
            return true;
          }
          if (localIt->second.argsPackElementKind == LocalInfo::Kind::Map) {
            typeNameOut = "map";
            return true;
          }
          if (localIt->second.argsPackElementKind == LocalInfo::Kind::Buffer) {
            typeNameOut = "Buffer";
            return true;
          }
          if (localIt->second.argsPackElementKind == LocalInfo::Kind::Reference &&
              localIt->second.referenceToBuffer) {
            typeNameOut = "Buffer";
            return true;
          }
          if (localIt->second.argsPackElementKind == LocalInfo::Kind::Pointer &&
              localIt->second.pointerToBuffer) {
            typeNameOut = "Buffer";
            return true;
          }
        }
      }
    }
    if (isSimpleCallName(receiverExpr, "dereference") && receiverExpr.args.size() == 1) {
      const Expr &targetExpr = receiverExpr.args.front();
      if (resolveDereferencedCollectionOrFileErrorReceiver(targetExpr)) {
        return true;
      }
    }
    auto isBareMapAccessReceiverProbeExpr = [&](const Expr &candidateExpr) {
      if (candidateExpr.kind != Expr::Kind::Call || candidateExpr.args.size() != 2) {
        return false;
      }
      std::string accessName;
      return getBuiltinArrayAccessName(candidateExpr, accessName) &&
             resolveMapAccessTargetInfo(candidateExpr.args.front(), localsIn).isMapTarget;
    };
    auto isBareMapTryAtReceiverProbeExpr = [&](const Expr &candidateExpr) {
      return candidateExpr.kind == Expr::Kind::Call && candidateExpr.args.size() == 2 &&
             isSimpleCallName(candidateExpr, "tryAt") &&
             resolveMapAccessTargetInfo(candidateExpr.args.front(), localsIn).isMapTarget;
    };
    const bool blocksExplicitMapReceiverProbeKindFallback =
        isExplicitMapReceiverProbeHelperExpr(receiverExpr);
    const bool blocksBareMapAccessReceiverProbeKindFallback =
        isBareMapAccessReceiverProbeExpr(receiverExpr);
    const bool blocksBareMapTryAtReceiverProbeKindFallback =
        isBareMapTryAtReceiverProbeExpr(receiverExpr);
    const bool blocksExplicitVectorReceiverProbeKindFallback =
        isExplicitVectorReceiverProbeHelperExpr(receiverExpr);
    const LocalInfo::ValueKind inferredKind =
        (inferExprKind && !blocksExplicitMapReceiverProbeKindFallback &&
         !blocksBareMapAccessReceiverProbeKindFallback &&
         !blocksBareMapTryAtReceiverProbeKindFallback &&
         !blocksExplicitVectorReceiverProbeKindFallback)
            ? inferExprKind(receiverExpr, localsIn)
            : LocalInfo::ValueKind::Unknown;
    typeNameOut = resolveMethodReceiverTypeNameFromCallExpr(receiverExpr, inferredKind, resolveExprPath);
    if (typeNameOut.empty() && !blocksExplicitMapReceiverProbeKindFallback &&
        !blocksBareMapAccessReceiverProbeKindFallback &&
        !blocksBareMapTryAtReceiverProbeKindFallback &&
        !blocksExplicitVectorReceiverProbeKindFallback && receiverExpr.isMethodCall &&
        receiverExpr.args.size() == 2) {
      std::string accessName;
      if (getBuiltinArrayAccessName(receiverExpr, accessName) &&
          inferExprKind && inferExprKind(receiverExpr.args.front(), localsIn) == LocalInfo::ValueKind::String) {
        typeNameOut = "i32";
      }
    }
    if (typeNameOut.empty()) {
      resolvedTypePathOut = resolveMethodReceiverStructTypePathFromCallExpr(
          receiverExpr, resolveExprPath(receiverExpr), importAliases, structNames);
    }
    return true;
  }

  typeNameOut = inferExprKind ? typeNameForValueKind(inferExprKind(receiverExpr, localsIn)) : "";
  return true;
}

} // namespace primec::ir_lowerer
