#include "EmitterBuiltinCallPathHelpersInternal.h"
#include "EmitterBuiltinMethodResolutionTypeInferenceInternal.h"
#include "EmitterHelpers.h"

#include <string_view>

namespace primec::emitter {

using BindingInfo = Emitter::BindingInfo;

namespace {

std::string inferSoaReceiverTypeFromBinding(const BindingInfo &binding) {
  auto classifyTypeText = [&](const std::string &typeText) -> std::string {
    std::string normalized = normalizeBindingTypeName(typeText);
    bool borrowed = false;
    while (true) {
      if (!normalized.empty() && normalized.front() == '/') {
        normalized.erase(normalized.begin());
      }
      if (normalized == "soa_vector" || normalized == "SoaVector" ||
          normalized == "std/collections/soa_vector" ||
          normalized == "std/collections/experimental_soa_vector/SoaVector" ||
          normalized.rfind("soa_vector<", 0) == 0 ||
          normalized.rfind("SoaVector<", 0) == 0 ||
          normalized.rfind("std/collections/experimental_soa_vector/SoaVector<", 0) == 0) {
        return borrowed ? "soa_vector_ref" : "soa_vector";
      }
      std::string base;
      std::string arg;
      if (!splitTemplateTypeName(normalized, base, arg)) {
        return "";
      }
      std::vector<std::string> args;
      if (!splitTopLevelTemplateArgs(arg, args) || args.empty()) {
        return "";
      }
      if ((base == "Reference" || base == "Pointer") && args.size() == 1) {
        borrowed = true;
        normalized = normalizeBindingTypeName(args.front());
        continue;
      }
      return "";
    }
  };

  std::string typeText = binding.typeName;
  if (!binding.typeTemplateArg.empty()) {
    typeText += "<" + binding.typeTemplateArg + ">";
  }
  return classifyTypeText(typeText);
}

std::string borrowedSoaMethodName(std::string_view methodName) {
  if (methodName == "count") {
    return "count_ref";
  }
  if (methodName == "get") {
    return "get_ref";
  }
  if (methodName == "ref") {
    return "ref_ref";
  }
  if (methodName == "to_aos") {
    return "to_aos_ref";
  }
  return std::string(methodName);
}

bool isCanonicalSoaWrapperMethodName(std::string_view methodName) {
  return methodName == "count" || methodName == "count_ref" ||
         methodName == "get" || methodName == "get_ref" ||
         methodName == "ref" || methodName == "ref_ref" ||
         methodName == "to_aos" || methodName == "to_aos_ref" ||
         methodName == "push" || methodName == "reserve";
}

std::string inferConcreteExperimentalSoaStructPathFromTypeText(std::string typeText) {
  std::string normalized = normalizeBindingTypeName(typeText);
  while (true) {
    std::string base;
    std::string arg;
    if (!splitTemplateTypeName(normalized, base, arg)) {
      return "";
    }
    if (base == "Reference" || base == "Pointer") {
      normalized = normalizeBindingTypeName(arg);
      continue;
    }
    if (base == "SoaVector" || base == "soa_vector" ||
        base == "std/collections/experimental_soa_vector/SoaVector" ||
        base == "std/collections/soa_vector") {
      std::string normalizedArg = normalizeBindingTypeName(arg);
      if (!normalizedArg.empty() && normalizedArg.front() == '/') {
        normalizedArg.erase(normalizedArg.begin());
      }
      return "/std/collections/experimental_soa_vector/SoaVector__" + normalizedArg;
    }
    return "";
  }
}

std::string inferConcreteExperimentalSoaStructPathFromBinding(const BindingInfo &binding) {
  std::string typeText = binding.typeName;
  if (!binding.typeTemplateArg.empty()) {
    typeText += "<" + binding.typeTemplateArg + ">";
  }
  return inferConcreteExperimentalSoaStructPathFromTypeText(typeText);
}

bool isConcreteExperimentalSoaVectorStructPath(std::string_view path) {
  return path.rfind("/std/collections/experimental_soa_vector/SoaVector__", 0) == 0;
}

std::string resolveConcreteSoaStructMethodPath(const MethodResolutionMetadataView &view,
                                               std::string_view structPath,
                                               std::string_view methodName) {
  if (!isConcreteExperimentalSoaVectorStructPath(structPath)) {
    return "";
  }
  if (isCanonicalSoaWrapperMethodName(methodName)) {
    return "";
  }
  const std::string concretePath =
      std::string(structPath) + "/" + std::string(methodName);
  if (hasDefinitionOrMetadata(view, concretePath)) {
    return concretePath;
  }
  return "";
}

std::string resolveConcreteSoaStructMethodPathFromReceiver(
    const Expr &receiver,
    const MethodResolutionMetadataView &view,
    const std::unordered_map<std::string, BindingInfo> &localTypes,
    std::string_view methodName) {
  auto tryResolvedStructPath = [&](const std::string &structPath) -> std::string {
    return resolveConcreteSoaStructMethodPath(view, structPath, methodName);
  };

  if (receiver.kind == Expr::Kind::Name) {
    auto localIt = localTypes.find(receiver.name);
    if (localIt != localTypes.end()) {
      const std::string resolved =
          tryResolvedStructPath(inferConcreteExperimentalSoaStructPathFromBinding(localIt->second));
      if (!resolved.empty()) {
        return resolved;
      }
    }
    return "";
  }

  if (receiver.kind != Expr::Kind::Call || receiver.args.empty()) {
    return "";
  }

  std::string receiverPath = resolveExprPath(receiver);
  if (!receiverPath.empty() && receiverPath.front() != '/') {
    receiverPath.insert(receiverPath.begin(), '/');
  }
  if (const std::string *returnedStructPath = findReturnStructMetadata(view, receiverPath);
      returnedStructPath != nullptr) {
    const std::string resolved = tryResolvedStructPath(*returnedStructPath);
    if (!resolved.empty()) {
      return resolved;
    }
  }

  char pointerOperator = '\0';
  if (getBuiltinPointerOperator(receiver, pointerOperator)) {
    return resolveConcreteSoaStructMethodPathFromReceiver(
        receiver.args.front(), view, localTypes, methodName);
  }

  return "";
}

} // namespace

bool resolveMethodCallPath(const Expr &call,
                           const std::unordered_map<std::string, const Definition *> &defMap,
                           const std::unordered_map<std::string, BindingInfo> &localTypes,
                           const std::unordered_map<std::string, std::string> &importAliases,
                           const std::unordered_map<std::string, std::string> &structTypeMap,
                           const std::unordered_map<std::string, ReturnKind> &returnKinds,
                           const std::unordered_map<std::string, std::string> &returnStructs,
                           std::string &resolvedOut) {
  resolvedOut.clear();
  if (!call.isMethodCall || call.args.empty()) {
    return false;
  }
  MethodResolutionMetadataView metadataView{
      defMap, importAliases, structTypeMap, returnKinds, returnStructs};
  if (isRemovedCollectionMethodAliasPath(call.name)) {
    std::string explicitPath = call.name;
    if (!explicitPath.empty() && explicitPath.front() != '/') {
      explicitPath.insert(explicitPath.begin(), '/');
    }
    const bool hasSamePathHelper =
        removedCollectionAliasNeedsDefinitionPath(explicitPath)
            ? defMap.count(explicitPath) > 0
            : hasDefinitionOrMetadata(metadataView, explicitPath);
    if (!hasSamePathHelper) {
      return false;
    }
  }

  std::string normalizedMethodName = call.name;
  if (!normalizedMethodName.empty() && normalizedMethodName.front() == '/') {
    normalizedMethodName.erase(normalizedMethodName.begin());
  }
  const bool isExplicitStdlibVectorMethod =
      normalizedMethodName.rfind("std/collections/vector/", 0) == 0;
  const bool isExplicitMapAliasMethod = normalizedMethodName.rfind("map/", 0) == 0;
  const bool isExplicitStdlibMapMethod =
      normalizedMethodName.rfind("std/collections/map/", 0) == 0;
  const bool isExplicitSoaAliasMethod = normalizedMethodName.rfind("soa_vector/", 0) == 0;
  const bool isExplicitStdlibSoaMethod =
      normalizedMethodName.rfind("std/collections/soa_vector/", 0) == 0;
  if (normalizedMethodName.rfind("array/", 0) == 0) {
    normalizedMethodName = normalizedMethodName.substr(std::string("array/").size());
  } else if (normalizedMethodName.rfind("std/collections/vector/", 0) == 0) {
    normalizedMethodName =
        normalizedMethodName.substr(std::string("std/collections/vector/").size());
  } else if (normalizedMethodName.rfind("soa_vector/", 0) == 0) {
    normalizedMethodName =
        normalizedMethodName.substr(std::string("soa_vector/").size());
  } else if (normalizedMethodName.rfind("std/collections/soa_vector/", 0) == 0) {
    normalizedMethodName =
        normalizedMethodName.substr(std::string("std/collections/soa_vector/").size());
  } else if (normalizedMethodName.rfind("map/", 0) == 0) {
    normalizedMethodName = normalizedMethodName.substr(std::string("map/").size());
  } else if (normalizedMethodName.rfind("std/collections/map/", 0) == 0) {
    normalizedMethodName =
        normalizedMethodName.substr(std::string("std/collections/map/").size());
  }
  if (isExplicitStdlibVectorMethod) {
    const std::string explicitPath =
        std::string("/") +
        "std/collections/vector/" +
        normalizedMethodName;
    if (hasDefinitionOrMetadata(metadataView, explicitPath)) {
      resolvedOut = explicitPath;
      return true;
    }
  }

  const Expr &receiver = call.args.front();
  auto preferredFileErrorHelperTarget = [&](std::string_view helperName) -> std::string {
    if (helperName == "why") {
      if (defMap.find("/std/file/FileError/why") != defMap.end()) {
        return "/std/file/FileError/why";
      }
      if (defMap.find("/FileError/why") != defMap.end()) {
        return "/FileError/why";
      }
      return "/file_error/why";
    }
    if (helperName == "is_eof") {
      if (defMap.find("/std/file/FileError/is_eof") != defMap.end()) {
        return "/std/file/FileError/is_eof";
      }
      if (defMap.find("/FileError/is_eof") != defMap.end()) {
        return "/FileError/is_eof";
      }
      if (defMap.find("/std/file/fileErrorIsEof") != defMap.end()) {
        return "/std/file/fileErrorIsEof";
      }
      return "";
    }
    if (helperName == "eof") {
      if (defMap.find("/std/file/FileError/eof") != defMap.end()) {
        return "/std/file/FileError/eof";
      }
      if (defMap.find("/FileError/eof") != defMap.end()) {
        return "/FileError/eof";
      }
      if (defMap.find("/std/file/fileReadEof") != defMap.end()) {
        return "/std/file/fileReadEof";
      }
      return "";
    }
    if (helperName == "status") {
      if (defMap.find("/std/file/FileError/status") != defMap.end()) {
        return "/std/file/FileError/status";
      }
      if (defMap.find("/std/file/fileErrorStatus") != defMap.end()) {
        return "/std/file/fileErrorStatus";
      }
      return "";
    }
    if (helperName == "result") {
      if (defMap.find("/std/file/FileError/result") != defMap.end()) {
        return "/std/file/FileError/result";
      }
      if (defMap.find("/std/file/fileErrorResult") != defMap.end()) {
        return "/std/file/fileErrorResult";
      }
      return "";
    }
    return "";
  };
  auto preferredImageErrorHelperTarget = [&](std::string_view helperName) -> std::string {
    if (helperName == "why") {
      if (defMap.find("/std/image/ImageError/why") != defMap.end()) {
        return "/std/image/ImageError/why";
      }
      if (defMap.find("/ImageError/why") != defMap.end()) {
        return "/ImageError/why";
      }
      return "";
    }
    if (helperName == "status") {
      if (defMap.find("/std/image/ImageError/status") != defMap.end()) {
        return "/std/image/ImageError/status";
      }
      if (defMap.find("/ImageError/status") != defMap.end()) {
        return "/ImageError/status";
      }
      if (defMap.find("/std/image/imageErrorStatus") != defMap.end()) {
        return "/std/image/imageErrorStatus";
      }
      return "";
    }
    if (helperName == "result") {
      if (defMap.find("/std/image/ImageError/result") != defMap.end()) {
        return "/std/image/ImageError/result";
      }
      if (defMap.find("/ImageError/result") != defMap.end()) {
        return "/ImageError/result";
      }
      if (defMap.find("/std/image/imageErrorResult") != defMap.end()) {
        return "/std/image/imageErrorResult";
      }
      return "";
    }
    return "";
  };
  auto preferredContainerErrorHelperTarget = [&](std::string_view helperName) -> std::string {
    if (helperName == "why") {
      if (defMap.find("/std/collections/ContainerError/why") != defMap.end()) {
        return "/std/collections/ContainerError/why";
      }
      if (defMap.find("/ContainerError/why") != defMap.end()) {
        return "/ContainerError/why";
      }
      return "";
    }
    if (helperName == "status") {
      if (defMap.find("/std/collections/ContainerError/status") != defMap.end()) {
        return "/std/collections/ContainerError/status";
      }
      if (defMap.find("/ContainerError/status") != defMap.end()) {
        return "/ContainerError/status";
      }
      if (defMap.find("/std/collections/containerErrorStatus") != defMap.end()) {
        return "/std/collections/containerErrorStatus";
      }
      return "";
    }
    if (helperName == "result") {
      if (defMap.find("/std/collections/ContainerError/result") != defMap.end()) {
        return "/std/collections/ContainerError/result";
      }
      if (defMap.find("/ContainerError/result") != defMap.end()) {
        return "/ContainerError/result";
      }
      if (defMap.find("/std/collections/containerErrorResult") != defMap.end()) {
        return "/std/collections/containerErrorResult";
      }
      return "";
    }
    return "";
  };
  auto preferredGfxErrorHelperTarget = [&](std::string_view helperName,
                                           const std::string &resolvedStructPath) -> std::string {
    auto helperForBasePath = [&](std::string_view basePath) -> std::string {
      const std::string helperPath = std::string(basePath) + "/" + std::string(helperName);
      if (defMap.find(helperPath) != defMap.end()) {
        return helperPath;
      }
      return "";
    };
    const std::string canonicalBase = "/std/gfx/GfxError";
    const std::string experimentalBase = "/std/gfx/experimental/GfxError";
    if (resolvedStructPath == canonicalBase) {
      return helperForBasePath(canonicalBase);
    }
    if (resolvedStructPath == experimentalBase) {
      return helperForBasePath(experimentalBase);
    }
    const bool hasCanonical =
        defMap.find(canonicalBase + "/" + std::string(helperName)) != defMap.end();
    const bool hasExperimental =
        defMap.find(experimentalBase + "/" + std::string(helperName)) != defMap.end();
    if (hasCanonical && !hasExperimental) {
      return helperForBasePath(canonicalBase);
    }
    if (!hasCanonical && hasExperimental) {
      return helperForBasePath(experimentalBase);
    }
    return "";
  };

  std::string typeName;
  bool borrowedSoaReceiver = false;
  if (receiver.kind == Expr::Kind::Name) {
    auto it = localTypes.find(receiver.name);
    if (it != localTypes.end()) {
      const std::string inferredSoaType = inferSoaReceiverTypeFromBinding(it->second);
      if (!inferredSoaType.empty()) {
        borrowedSoaReceiver = inferredSoaType == "soa_vector_ref";
        typeName = "soa_vector";
      } else {
        typeName = it->second.typeName;
      }
    } else {
      if (receiver.name == "FileError" &&
          (normalizedMethodName == "why" || normalizedMethodName == "is_eof" ||
           normalizedMethodName == "eof" || normalizedMethodName == "status" ||
           normalizedMethodName == "result")) {
        resolvedOut = preferredFileErrorHelperTarget(normalizedMethodName);
        return !resolvedOut.empty();
      }
      if (receiver.name == "ImageError" &&
          (normalizedMethodName == "why" || normalizedMethodName == "status" ||
           normalizedMethodName == "result")) {
        resolvedOut = preferredImageErrorHelperTarget(normalizedMethodName);
        return !resolvedOut.empty();
      }
      if (receiver.name == "ContainerError" &&
          (normalizedMethodName == "why" || normalizedMethodName == "status" ||
           normalizedMethodName == "result")) {
        resolvedOut = preferredContainerErrorHelperTarget(normalizedMethodName);
        return !resolvedOut.empty();
      }
      if (receiver.name == "GfxError" &&
          (normalizedMethodName == "why" || normalizedMethodName == "status" ||
           normalizedMethodName == "result")) {
        std::string resolvedStructPath = resolveExprPath(receiver);
        if (findStructTypeMetadata(metadataView, resolvedStructPath) == nullptr) {
          resolvedStructPath = resolveTypePath(receiver.name, receiver.namespacePrefix);
        }
        resolvedOut = preferredGfxErrorHelperTarget(normalizedMethodName, resolvedStructPath);
        if (!resolvedOut.empty()) {
          return true;
        }
      }
      std::string resolvedReceiverPath = resolveExprPath(receiver);
      if (findStructTypeMetadata(metadataView, resolvedReceiverPath) != nullptr ||
          defMap.find(resolvedReceiverPath + "/" + normalizedMethodName) != defMap.end()) {
        resolvedOut = preferCanonicalMapMethodHelperPath(
            metadataView, resolvedReceiverPath + "/" + normalizedMethodName);
        return true;
      }
      std::string resolvedType = resolveTypePath(receiver.name, receiver.namespacePrefix);
      auto importIt = importAliases.find(receiver.name);
      if (findStructTypeMetadata(metadataView, resolvedType) == nullptr &&
          importIt != importAliases.end()) {
        resolvedType = normalizeMapImportAliasPath(importIt->second);
      }
      if (findStructTypeMetadata(metadataView, resolvedType) != nullptr) {
        resolvedOut =
            preferCanonicalMapMethodHelperPath(metadataView, resolvedType + "/" + normalizedMethodName);
        return true;
      }
      return false;
    }
  } else if (receiver.kind == Expr::Kind::Call && !receiver.isMethodCall && !receiver.isBinding) {
    std::string resolved = resolveExprPath(receiver);
    auto normalizeResolvedPath = [](std::string path) {
      if (!path.empty() && path.front() != '/') {
        path.insert(path.begin(), '/');
      }
      return path;
    };
    auto hasStructPath = [&](const std::string &path) {
      return findStructTypeMetadata(metadataView, path) != nullptr;
    };
    auto importIt = importAliases.find(receiver.name);
    if (!hasStructPath(resolved) && importIt != importAliases.end()) {
      resolved = importIt->second;
    }
    const std::string normalizedResolved = normalizeResolvedPath(resolved);
    if (!hasStructPath(resolved) && hasStructPath(normalizedResolved)) {
      resolved = normalizedResolved;
    }
    if (hasStructPath(resolved)) {
      resolvedOut = normalizeResolvedPath(resolved) + "/" + normalizedMethodName;
      return true;
    }
    resolvedOut = resolveConcreteSoaStructMethodPathFromReceiver(
        receiver, metadataView, localTypes, normalizedMethodName);
    if (!resolvedOut.empty()) {
      return true;
    }
    typeName = inferMethodResolutionPrimitiveTypeName(receiver, metadataView, localTypes);
    if (typeName == "soa_vector_ref") {
      borrowedSoaReceiver = true;
      typeName = "soa_vector";
    }
  } else {
    resolvedOut = resolveConcreteSoaStructMethodPathFromReceiver(
        receiver, metadataView, localTypes, normalizedMethodName);
    if (!resolvedOut.empty()) {
      return true;
    }
    typeName = inferMethodResolutionPrimitiveTypeName(receiver, metadataView, localTypes);
    if (typeName == "soa_vector_ref") {
      borrowedSoaReceiver = true;
      typeName = "soa_vector";
    }
  }

  if (typeName.empty()) {
    return false;
  }
  if (typeName == "File") {
    resolvedOut = preferredFileMethodTargetLocal(normalizedMethodName, defMap);
    return true;
  }
  if (typeName == "FileError" &&
      (normalizedMethodName == "why" || normalizedMethodName == "is_eof" ||
       normalizedMethodName == "status" || normalizedMethodName == "result")) {
    resolvedOut = preferredFileErrorHelperTarget(normalizedMethodName);
    return !resolvedOut.empty();
  }
  if (typeName == "ImageError" &&
      (normalizedMethodName == "why" || normalizedMethodName == "status" ||
       normalizedMethodName == "result")) {
    resolvedOut = preferredImageErrorHelperTarget(normalizedMethodName);
    return !resolvedOut.empty();
  }
  if (typeName == "ContainerError" &&
      (normalizedMethodName == "why" || normalizedMethodName == "status" ||
       normalizedMethodName == "result")) {
    resolvedOut = preferredContainerErrorHelperTarget(normalizedMethodName);
    return !resolvedOut.empty();
  }
  if (typeName == "GfxError" &&
      (normalizedMethodName == "why" || normalizedMethodName == "status" ||
       normalizedMethodName == "result")) {
    std::string resolvedStructPath = resolveTypePath(typeName, call.namespacePrefix);
    auto importIt = importAliases.find(typeName);
    if (findStructTypeMetadata(metadataView, resolvedStructPath) == nullptr &&
        importIt != importAliases.end()) {
      resolvedStructPath = importIt->second;
    }
    resolvedOut = preferredGfxErrorHelperTarget(normalizedMethodName, resolvedStructPath);
    if (!resolvedOut.empty()) {
      return true;
    }
  }
  if (typeName == "Pointer" || typeName == "Reference") {
    return false;
  }
  if (isPrimitiveBindingTypeName(typeName)) {
    resolvedOut = "/" + normalizeBindingTypeName(typeName) + "/" + normalizedMethodName;
    return true;
  }

  typeName = normalizeMapImportAliasPath(typeName);
  std::string resolvedType = resolveTypePath(typeName, call.namespacePrefix);
  if (findReturnKindMetadata(metadataView, resolvedType) == nullptr) {
    auto importIt = importAliases.find(typeName);
    if (importIt != importAliases.end()) {
      resolvedType = normalizeMapImportAliasPath(importIt->second);
    }
  }
  if (resolvedType == "/vector" || resolvedType == "vector") {
    const bool isCountLikeMethod = normalizedMethodName == "count";
    const bool isCapacityLikeMethod = normalizedMethodName == "capacity";
    if (isCountLikeMethod || isCapacityLikeMethod) {
      const std::string canonicalPath = "/std/collections/vector/" + normalizedMethodName;
      if (isExplicitStdlibVectorMethod &&
          !hasDefinitionOrMetadata(metadataView, canonicalPath)) {
        return false;
      }
      if (!hasDefinitionOrMetadata(metadataView, canonicalPath)) {
        return false;
      }
      resolvedOut = canonicalPath;
      return true;
    }
  }
  if (resolvedType == "/soa_vector" || resolvedType == "soa_vector") {
    const std::string helperName =
        borrowedSoaReceiver ? borrowedSoaMethodName(normalizedMethodName)
                            : normalizedMethodName;
    const std::string canonicalPath = "/std/collections/soa_vector/" + helperName;
    const std::string aliasPath = "/soa_vector/" + helperName;
    if (isExplicitStdlibSoaMethod) {
      if (!hasDefinitionOrMetadata(metadataView, canonicalPath)) {
        return false;
      }
      resolvedOut = canonicalPath;
      return true;
    }
    if (isExplicitSoaAliasMethod) {
      if (hasDefinitionOrMetadata(metadataView, aliasPath)) {
        resolvedOut = aliasPath;
        return true;
      }
      if (!hasDefinitionOrMetadata(metadataView, canonicalPath)) {
        return false;
      }
      resolvedOut = canonicalPath;
      return true;
    }
    if (hasDefinitionOrMetadata(metadataView, canonicalPath)) {
      resolvedOut = canonicalPath;
      return true;
    }
    if (hasDefinitionOrMetadata(metadataView, aliasPath)) {
      resolvedOut = aliasPath;
      return true;
    }
  }
  if (resolvedType == "/map" || resolvedType == "map") {
    const std::string aliasPath = "/map/" + normalizedMethodName;
    const std::string canonicalPath = "/std/collections/map/" + normalizedMethodName;
    const bool isMapHelperMethod = isCanonicalMapHelperName(normalizedMethodName);
    const bool isRemovedMapSlashMethod =
        isRemovedMapSlashMethodMetadataHelperName(normalizedMethodName);
    const bool hasAliasHelperDefinition =
        hasDefinitionOrMetadata(metadataView, aliasPath);
    const bool hasCanonicalHelperDefinition =
        hasDefinitionOrMetadata(metadataView, canonicalPath);
    if (isMapHelperMethod) {
      if (isRemovedMapSlashMethod &&
          (isExplicitMapAliasMethod || isExplicitStdlibMapMethod)) {
        return false;
      }
      if (isExplicitMapAliasMethod) {
        if (!hasAliasHelperDefinition) {
          return false;
        }
        resolvedOut = aliasPath;
        return true;
      }
      if (isExplicitStdlibMapMethod) {
        if (!hasCanonicalHelperDefinition) {
          return false;
        }
        resolvedOut = canonicalPath;
        return true;
      }
      if (!hasAliasHelperDefinition && !hasCanonicalHelperDefinition) {
        return false;
      }
    }
    if (isExplicitMapAliasMethod) {
      if (!hasDefinitionOrMetadata(metadataView, aliasPath)) {
        return false;
      }
      resolvedOut = aliasPath;
      return true;
    }
    if (isExplicitStdlibMapMethod) {
      if (!hasDefinitionOrMetadata(metadataView, canonicalPath)) {
        return false;
      }
      resolvedOut = canonicalPath;
      return true;
    }
  }
  resolvedOut =
      preferCanonicalMapMethodHelperPath(metadataView, resolvedType + "/" + normalizedMethodName);
  return true;
}

} // namespace primec::emitter
