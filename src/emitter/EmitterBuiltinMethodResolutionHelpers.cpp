#include "EmitterBuiltinCallPathHelpersInternal.h"
#include "EmitterBuiltinMethodResolutionTypeInferenceInternal.h"
#include "EmitterHelpers.h"

#include <string_view>

namespace primec::emitter {

using BindingInfo = Emitter::BindingInfo;

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

bool isRemovedCollectionMethodAlias(const std::string &rawMethodName) {
  std::string candidate = rawMethodName;
  if (!candidate.empty() && candidate.front() == '/') {
    candidate.erase(candidate.begin());
  }
  if (candidate.rfind("array/", 0) == 0) {
    return isRemovedVectorCompatibilityHelper(
        std::string_view(candidate).substr(std::string_view("array/").size()));
  }
  if (candidate.rfind("vector/", 0) == 0) {
    return isRemovedVectorCompatibilityHelper(
        std::string_view(candidate).substr(std::string_view("vector/").size()));
  }
  if (candidate.rfind("std/collections/vector/", 0) == 0) {
    return isRemovedVectorCompatibilityHelper(
        std::string_view(candidate).substr(std::string_view("std/collections/vector/").size()));
  }
  if (candidate.rfind("map/", 0) == 0) {
    return isRemovedMapCompatibilityHelper(
        std::string_view(candidate).substr(std::string_view("map/").size()));
  }
  if (candidate.rfind("std/collections/map/", 0) == 0) {
    return isRemovedMapCompatibilityHelper(
        std::string_view(candidate).substr(std::string_view("std/collections/map/").size()));
  }
  return false;
}

bool removedCollectionAliasNeedsDefinition(std::string_view rawMethodName) {
  return rawMethodName == "/map/count" ||
         rawMethodName == "/std/collections/map/count" ||
         rawMethodName == "/array/count" ||
         rawMethodName == "/array/capacity" ||
         rawMethodName == "/std/collections/vector/count" ||
         rawMethodName == "/std/collections/vector/capacity";
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
  if (isRemovedCollectionMethodAlias(call.name)) {
    std::string explicitPath = call.name;
    if (!explicitPath.empty() && explicitPath.front() != '/') {
      explicitPath.insert(explicitPath.begin(), '/');
    }
    const bool hasSamePathHelper =
        removedCollectionAliasNeedsDefinition(explicitPath)
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
  const bool isExplicitVectorAliasMethod = normalizedMethodName.rfind("vector/", 0) == 0;
  const bool isExplicitStdlibVectorMethod =
      normalizedMethodName.rfind("std/collections/vector/", 0) == 0;
  const bool isExplicitMapAliasMethod = normalizedMethodName.rfind("map/", 0) == 0;
  const bool isExplicitStdlibMapMethod =
      normalizedMethodName.rfind("std/collections/map/", 0) == 0;
  if (isExplicitVectorAliasMethod) {
    return false;
  }
  if (normalizedMethodName.rfind("vector/", 0) == 0) {
    normalizedMethodName = normalizedMethodName.substr(std::string("vector/").size());
  } else if (normalizedMethodName.rfind("array/", 0) == 0) {
    normalizedMethodName = normalizedMethodName.substr(std::string("array/").size());
  } else if (normalizedMethodName.rfind("std/collections/vector/", 0) == 0) {
    normalizedMethodName =
        normalizedMethodName.substr(std::string("std/collections/vector/").size());
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
  if (receiver.kind == Expr::Kind::Name) {
    auto it = localTypes.find(receiver.name);
    if (it != localTypes.end()) {
      typeName = it->second.typeName;
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
    typeName = inferMethodResolutionPrimitiveTypeName(receiver, metadataView, localTypes);
  } else {
    typeName = inferMethodResolutionPrimitiveTypeName(receiver, metadataView, localTypes);
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
    }
  }
  if (resolvedType == "/map" || resolvedType == "map") {
    const std::string aliasPath = "/map/" + normalizedMethodName;
    const std::string canonicalPath = "/std/collections/map/" + normalizedMethodName;
    const bool isMapHelperMethod = normalizedMethodName == "count" ||
                                   normalizedMethodName == "contains" ||
                                   normalizedMethodName == "tryAt" ||
                                   normalizedMethodName == "at" ||
                                   normalizedMethodName == "at_unsafe";
    const bool hasAliasHelperDefinition =
        hasDefinitionOrMetadata(metadataView, aliasPath);
    const bool hasCanonicalHelperDefinition =
        hasDefinitionOrMetadata(metadataView, canonicalPath);
    if (isMapHelperMethod) {
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
