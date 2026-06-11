// soa-surface-audit: exempt
#include "EmitterBuiltinCallPathHelpersInternal.h"
#include "EmitterBuiltinMethodResolutionTypeInferenceInternal.h"
#include "EmitterCollectionSurfaceMetadata.h"
#include "EmitterHelpers.h"

#include <string_view>
#include "primec/StdlibCollectionPaths.h"

namespace primec::emitter {

using BindingInfo = Emitter::BindingInfo;

namespace {

std::string publishedSurfaceHelperPathForRawMethodName(const StdlibSurfaceMetadata &metadata,
                                                       std::string_view rawMethodName) {
  if (rawMethodName.empty()) {
    return {};
  }
  std::string normalizedRaw(rawMethodName);
  if (!normalizedRaw.empty() && normalizedRaw.front() != '/') {
    normalizedRaw.insert(normalizedRaw.begin(), '/');
  }
  std::string memberName;
  if (!resolvePublishedCollectionSurfacePathMemberName(
          normalizedRaw,
          metadata,
          true,
          memberName)) {
    return {};
  }
  auto pathForRoot = [&](std::string_view root) {
    std::string normalizedRoot(root);
    if (!normalizedRoot.empty() && normalizedRoot.front() != '/') {
      normalizedRoot.insert(normalizedRoot.begin(), '/');
    }
    if (normalizedRoot.empty() ||
        normalizedRaw.size() <= normalizedRoot.size() ||
        normalizedRaw.rfind(normalizedRoot, 0) != 0 ||
        normalizedRaw[normalizedRoot.size()] != '/') {
      return std::string{};
    }
    return normalizedRoot + "/" + memberName;
  };
  if (std::string canonicalPath = pathForRoot(metadata.canonicalPath);
      !canonicalPath.empty()) {
    return canonicalPath;
  }
  for (const std::string_view alias : metadata.importAliasSpellings) {
    if (std::string aliasPath = pathForRoot(alias); !aliasPath.empty()) {
      return aliasPath;
    }
  }
  return {};
}

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
          normalized == collection_paths::memberPathBare(collection_paths::kSoaFolder, collection_paths::kSoaVectorTypeName) ||
          normalized.rfind("soa_vector<", 0) == 0 ||
          normalized.rfind("SoaVector<", 0) == 0 ||
          normalized.rfind(collection_paths::memberPathBare(
                               collection_paths::kSoaFolder,
                               collection_paths::kSoaVectorTypeName) +
                               "<",
                           0) == 0) {
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
        base == collection_paths::memberPathBare(collection_paths::kSoaFolder, collection_paths::kSoaVectorTypeName) ||
        base == "std/collections/soa_vector") {
      std::string normalizedArg = normalizeBindingTypeName(arg);
      if (!normalizedArg.empty() && normalizedArg.front() == '/') {
        normalizedArg.erase(normalizedArg.begin());
      }
      return collection_paths::specializedTypePrefix(collection_paths::kSoaFolder, collection_paths::kSoaVectorTypeName) + normalizedArg;
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
  return path.rfind(collection_paths::specializedTypePrefix(collection_paths::kSoaFolder, collection_paths::kSoaVectorTypeName), 0) == 0;
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
  const auto *vectorHelperMetadata =
      emitterCollectionSurfaceMetadata(EmitterCollectionSurface::VectorHelpers);
  const auto *keyValueHelperMetadata =
      emitterCollectionSurfaceMetadata(EmitterCollectionSurface::KeyValueHelpers);
  std::string explicitVectorMethodName;
  const bool isExplicitStdlibVectorMethod =
      vectorHelperMetadata != nullptr &&
      resolvePublishedCollectionSurfacePathMemberName(
          normalizedMethodName,
          *vectorHelperMetadata,
          false,
          explicitVectorMethodName);
  std::string explicitMapMethodName;
  const bool isExplicitMapMethod =
      keyValueHelperMetadata != nullptr &&
      resolvePublishedCollectionSurfacePathMemberName(
          normalizedMethodName,
          *keyValueHelperMetadata,
          true,
          explicitMapMethodName);
  const bool isExplicitStdlibSoaMethod =
      normalizedMethodName.rfind("std/collections/soa_vector/", 0) == 0;
  if (normalizedMethodName.rfind("array/", 0) == 0) {
    normalizedMethodName = normalizedMethodName.substr(std::string("array/").size());
  } else if (isExplicitStdlibVectorMethod) {
    normalizedMethodName = explicitVectorMethodName;
  } else if (normalizedMethodName.rfind("std/collections/soa_vector/", 0) == 0) {
    normalizedMethodName =
        normalizedMethodName.substr(std::string("std/collections/soa_vector/").size());
  } else if (isExplicitMapMethod) {
    normalizedMethodName = explicitMapMethodName;
  }
  if (isExplicitStdlibVectorMethod) {
    const std::string explicitPath =
        publishedCollectionSurfaceHelperPath(*vectorHelperMetadata,
                                             normalizedMethodName);
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
        resolvedOut = resolvedReceiverPath + "/" + normalizedMethodName;
        return true;
      }
      std::string resolvedType = resolveTypePath(receiver.name, receiver.namespacePrefix);
      auto importIt = importAliases.find(receiver.name);
      if (findStructTypeMetadata(metadataView, resolvedType) == nullptr &&
          importIt != importAliases.end()) {
        resolvedType = importIt->second;
      }
      if (findStructTypeMetadata(metadataView, resolvedType) != nullptr) {
        resolvedOut = resolvedType + "/" + normalizedMethodName;
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
      if (isConcreteExperimentalSoaVectorStructPath(resolved) &&
          isCanonicalSoaWrapperMethodName(normalizedMethodName)) {
        typeName = "soa_vector";
      } else {
        resolvedOut = normalizeResolvedPath(resolved) + "/" + normalizedMethodName;
        return true;
      }
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

  std::string resolvedType = resolveTypePath(typeName, call.namespacePrefix);
  if (findReturnKindMetadata(metadataView, resolvedType) == nullptr) {
    auto importIt = importAliases.find(typeName);
    if (importIt != importAliases.end()) {
      resolvedType = importIt->second;
    }
  }
  const bool isConcreteSoaWrapperReceiver =
      isConcreteExperimentalSoaVectorStructPath(resolvedType) &&
      isCanonicalSoaWrapperMethodName(normalizedMethodName);
  if (resolvedType == "/vector" || resolvedType == "vector") {
    const bool isCountLikeMethod = normalizedMethodName == "count";
    const bool isCapacityLikeMethod = normalizedMethodName == "capacity";
    if (isCountLikeMethod || isCapacityLikeMethod) {
      const std::string canonicalPath =
          vectorHelperMetadata == nullptr
              ? std::string{}
              : publishedCollectionSurfaceHelperPath(
                    *vectorHelperMetadata,
                    normalizedMethodName);
      if (canonicalPath.empty()) {
        return false;
      }
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
  if (isConcreteSoaWrapperReceiver || resolvedType == "/soa_vector" ||
      resolvedType == "soa_vector") {
    const std::string helperName =
        borrowedSoaReceiver ? borrowedSoaMethodName(normalizedMethodName)
                            : normalizedMethodName;
    const std::string canonicalPath = "/std/collections/soa_vector/" + helperName;
    if (isExplicitStdlibSoaMethod) {
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
  }
  if (normalizeBindingTypeName(resolvedType) == "map" &&
      keyValueHelperMetadata != nullptr) {
    const std::string explicitMapPath =
        publishedSurfaceHelperPathForRawMethodName(*keyValueHelperMetadata,
                                                   call.name);
    const std::string canonicalPath =
        publishedCollectionSurfaceHelperPath(*keyValueHelperMetadata,
                                             normalizedMethodName);
    const bool isCollectionPairHelperMethod =
        isCanonicalKeyValueHelperName(normalizedMethodName);
    const bool isRemovedKeyValueSlashMethod =
        isRemovedKeyValueSlashMethodMetadataHelperName(normalizedMethodName);
    const bool hasCanonicalHelperDefinition =
        hasDefinitionOrMetadata(metadataView, canonicalPath);
    if (isCollectionPairHelperMethod) {
      if (isRemovedKeyValueSlashMethod && isExplicitMapMethod) {
        return false;
      }
      if (isExplicitMapMethod) {
        if (explicitMapPath.empty() ||
            !hasDefinitionOrMetadata(metadataView, explicitMapPath)) {
          return false;
        }
        resolvedOut = explicitMapPath;
        return true;
      }
      if (!hasCanonicalHelperDefinition) {
        return false;
      }
    }
    if (isExplicitMapMethod) {
      if (explicitMapPath.empty() ||
          !hasDefinitionOrMetadata(metadataView, explicitMapPath)) {
        return false;
      }
      resolvedOut = explicitMapPath;
      return true;
    }
  }
  resolvedOut = resolvedType + "/" + normalizedMethodName;
  return true;
}

} // namespace primec::emitter
