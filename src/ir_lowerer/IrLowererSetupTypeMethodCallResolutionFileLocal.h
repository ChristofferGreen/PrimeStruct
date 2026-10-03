#pragma once

// Helpers shared by the IrLowererSetupTypeMethodCallResolution*.cpp units (split out of
// IrLowererSetupTypeMethodCallResolution.cpp without changes).
#include "primec/ir_lowerer/IrLowererSetupTypeHelpers.h"
#include <cctype>
#include <functional>
#include <string_view>
#include <utility>
#include "primec/ir_lowerer/IrLowererCallHelpers.h"
#include "primec/ir_lowerer/IrLowererHelpers.h"
#include "primec/ir_lowerer/IrLowererSetupTypeCollectionHelpers.h"
#include "IrLowererSetupTypeReceiverTargetHelpers.h"
#include "primec/ir_lowerer/IrLowererStructTypeHelpers.h"
#include "primec/ir_lowerer/IrLowererTemplateTypeParseHelpers.h"
#include "primec/ir/StdlibCollectionPaths.h"
#include "primec/ir_lowerer/IrLowererLegacyCollectionBranchCounters.h"
#include "primec/support/CollectionHelperNames.h"

namespace primec::ir_lowerer {

namespace ir_lowerer_setup_type_method_call_resolution_file_local {

inline std::string describeMethodCallExpr(const Expr &expr) {
  if (!expr.name.empty() && expr.name.front() == '/') {
    return expr.name;
  }
  if (!expr.namespacePrefix.empty()) {
    std::string displayName = expr.namespacePrefix;
    if (!displayName.empty() && displayName.front() != '/') {
      displayName.insert(displayName.begin(), '/');
    }
    displayName += "/" + expr.name;
    return displayName;
  }
  if (!expr.name.empty()) {
    return expr.name;
  }
  return "<unnamed>";
}

inline bool isKeyValueConstructorDirectTargetPath(std::string path) {
  const size_t specializationSuffix = path.find("__t");
  if (specializationSuffix != std::string::npos) {
    path.erase(specializationSuffix);
  }
  const size_t overloadSuffix = path.find("__ov");
  if (overloadSuffix != std::string::npos) {
    path.erase(overloadSuffix);
  }
  return path == canonicalKeyValueConstructorPath();
}

inline const StdlibSurfaceMetadata *keyValueConstructorSurfaceMetadataLocal() {
  return keyValueConstructorSurfaceMetadata();
}

inline std::string findKeyValueConstructorBridgePathChoiceBySource(
    const SemanticProgram *semanticProgram,
    const Expr &expr) {
  if (semanticProgram == nullptr || expr.sourceLine <= 0 || expr.sourceColumn <= 0) {
    return {};
  }
  const StdlibSurfaceMetadata *metadata = keyValueConstructorSurfaceMetadataLocal();
  if (metadata == nullptr) {
    return {};
  }
  const auto bridgePathChoices = semanticProgramBridgePathChoiceView(*semanticProgram);
  for (const auto *entry : bridgePathChoices) {
    if (entry == nullptr ||
        entry->sourceLine != expr.sourceLine ||
        entry->sourceColumn != expr.sourceColumn ||
        semanticProgramBridgePathChoiceStdlibSurfaceId(*entry) !=
            metadata->id ||
        entry->chosenPathId == InvalidSymbolId) {
      continue;
    }
    const std::string_view chosenPath =
        semanticProgramResolveCallTargetString(*semanticProgram, entry->chosenPathId);
    if (!chosenPath.empty()) {
      return std::string(chosenPath);
    }
  }
  return {};
}

inline std::string extractMethodLeafName(const std::string &methodPath) {
  if (methodPath.empty()) {
    return "";
  }
  const size_t slash = methodPath.find_last_of('/');
  if (slash == std::string::npos) {
    return methodPath;
  }
  return methodPath.substr(slash + 1);
}

inline bool isExperimentalSoaVectorSpecializedStructPath(std::string_view path) {
  return path.starts_with(collection_paths::specializedTypePrefix(collection_paths::kSoaFolder, collection_paths::kSoaVectorTypeName)) ||
         path.starts_with(collection_paths::specializedTypePrefixBare(collection_paths::kSoaFolder, collection_paths::kSoaVectorTypeName)) ||
         path.starts_with("SoaVector__");
}

inline bool isBuiltinFileHandleMethodName(std::string_view methodName) {
  return methodName == "write" || methodName == "write_line" ||
         methodName == "write_byte" || methodName == "read_byte" ||
         methodName == "write_bytes" || methodName == "flush" ||
         methodName == "close";
}

inline std::string resolveSpecializedExperimentalSoaVectorStructPath(
    const std::string &typeText) {
  std::string normalized = trimTemplateTypeText(typeText);
  while (true) {
    if (!normalized.empty() && normalized.front() != '/') {
      normalized.insert(normalized.begin(), '/');
    }
    if (isExperimentalSoaVectorSpecializedStructPath(normalized)) {
      return normalized;
    }

    std::string base;
    std::string argList;
    if (!splitTemplateTypeName(normalized, base, argList)) {
      return "";
    }

    const std::string normalizedBase =
        normalizeDeclaredCollectionTypeBase(trimTemplateTypeText(base));
    if ((normalizedBase == "Reference" || normalizedBase == "Pointer") &&
        !argList.empty()) {
      std::vector<std::string> wrappedArgs;
      if (!splitTemplateArgs(argList, wrappedArgs) || wrappedArgs.size() != 1) {
        return "";
      }
      normalized = trimTemplateTypeText(wrappedArgs.front());
      continue;
    }

    if (normalizedBase != "soa" || argList.empty()) {
      return "";
    }

    std::vector<std::string> templateArgs;
    if (!splitTemplateArgs(argList, templateArgs) || templateArgs.size() != 1) {
      return "";
    }

    std::string normalizedArg = trimTemplateTypeText(templateArgs.front());
    if (!normalizedArg.empty() && normalizedArg.front() == '/') {
      normalizedArg.erase(normalizedArg.begin());
    }
    return specializedExperimentalSoaVectorStructPathForElementType(
        normalizedArg);
  }
}

inline bool matchesGeneratedDefinitionFamilyPath(const std::string &candidatePath,
                                          const std::string &targetPath) {
  if (candidatePath == targetPath) {
    return true;
  }

  auto hasGeneratedTerminalSuffix = [&](size_t suffixPos) {
    if (suffixPos >= candidatePath.size()) {
      return false;
    }
    const bool isSpecialization =
        candidatePath.compare(suffixPos, 3, "__t") == 0;
    const bool isOverload =
        candidatePath.compare(suffixPos, 4, "__ov") == 0;
    return (isSpecialization || isOverload) &&
           candidatePath.find('/', suffixPos) == std::string::npos;
  };
  auto hasTerminalLeafOrGeneratedSuffix = [&](size_t leafEnd) {
    return leafEnd == candidatePath.size() ||
           hasGeneratedTerminalSuffix(leafEnd);
  };

  if (candidatePath.rfind(targetPath, 0) == 0 &&
      hasGeneratedTerminalSuffix(targetPath.size())) {
    return true;
  }

  const size_t slash = targetPath.find_last_of('/');
  if (slash == std::string::npos || slash == 0) {
    return false;
  }

  const std::string parentPath = targetPath.substr(0, slash);
  const std::string leafPath = targetPath.substr(slash);
  if (candidatePath.rfind(parentPath, 0) != 0) {
    return false;
  }

  const size_t specializationPos = parentPath.size();
  const bool specializesParent =
      candidatePath.compare(specializationPos, 3, "__t") == 0 ||
      candidatePath.compare(specializationPos, 4, "__ov") == 0;
  if (!specializesParent) {
    return false;
  }

  const size_t leafPos = candidatePath.find(leafPath, specializationPos);
  if (leafPos == std::string::npos ||
      candidatePath.compare(leafPos, leafPath.size(), leafPath) != 0) {
    return false;
  }
  return hasTerminalLeafOrGeneratedSuffix(leafPos + leafPath.size());
}

inline bool blocksSyntheticCollectionFallbackDirectTarget(const std::string &targetPath) {
  const std::string normalized = normalizeCollectionHelperPath(targetPath);
  return normalized.rfind(vectorBuiltinStructNormalizedPath() + "/", 0) == 0 ||
         normalized.rfind(collectionMemberRoot("vector"), 0) == 0 ||
         normalized.rfind(keyValueCollectionAliasRoot() + "/", 0) == 0 ||
         normalized.rfind(collectionMemberRoot("map"), 0) == 0 ||
         normalized.rfind(vectorBackingMemberRoot(), 0) == 0 ||
         normalized.rfind(experimentalCollectionMemberRoot("map"), 0) == 0 ||
         normalized.rfind(collection_paths::modulePrefix(collection_paths::kExperimentalSoaVectorFolder), 0) == 0;
}

inline bool isCollectionVectorMetadataMethodPath(const std::string &methodPath) {
  const std::string leaf = extractMethodLeafName(methodPath);
  return leaf == "field_count" || leaf == "field_capacity" ||
         leaf == "set_field_count" || leaf == "set_field_capacity";
}

inline bool isCollectionVectorOwnerPath(const std::string &path) {
  const std::string normalized = normalizeCollectionHelperPath(path);
  return isExperimentalCollectionTypeName(normalized, "vector", "Vector");
}

inline std::string unwrapSemanticReceiverTypeText(std::string typeText) {
  typeText = trimTemplateTypeText(std::move(typeText));
  while (true) {
    std::string base;
    std::string argList;
    if (!splitTemplateTypeName(typeText, base, argList)) {
      return typeText;
    }
    const std::string normalizedBase =
        normalizeDeclaredCollectionTypeBase(trimTemplateTypeText(base));
    if (normalizedBase != "Reference" && normalizedBase != "Pointer") {
      return typeText;
    }
    std::vector<std::string> args;
    if (!splitTemplateArgs(argList, args) || args.size() != 1) {
      return typeText;
    }
    typeText = trimTemplateTypeText(args.front());
  }
}

inline std::string findSemanticProductMethodCallReceiverTypeText(
    const SemanticProgram *semanticProgram,
    const Expr &expr) {
  if (semanticProgram == nullptr || expr.semanticNodeId == 0) {
    return "";
  }
  const auto methodCallTargets =
      semanticProgramMethodCallTargetView(*semanticProgram);
  for (const auto *entry : methodCallTargets) {
    if (entry == nullptr || entry->semanticNodeId != expr.semanticNodeId) {
      continue;
    }
    if (entry->receiverTypeTextId != InvalidSymbolId) {
      const std::string_view internedTypeText =
          semanticProgramResolveCallTargetString(*semanticProgram,
                                                 entry->receiverTypeTextId);
      if (!internedTypeText.empty()) {
        return std::string(internedTypeText);
      }
    }
    return entry->receiverTypeText;
  }
  return "";
}

inline std::string buildReceiverMethodTargetPath(const std::string &receiverPath,
                                          const std::string &explicitMethodPath) {
  if (receiverPath.empty()) {
    return "";
  }
  const std::string methodLeaf = extractMethodLeafName(explicitMethodPath);
  if (methodLeaf.empty()) {
    return "";
  }
  const std::string methodSuffix = "/" + methodLeaf;
  if (receiverPath.ends_with(methodSuffix)) {
    return receiverPath;
  }
  return receiverPath + methodSuffix;
}

} // namespace ir_lowerer_setup_type_method_call_resolution_file_local
} // namespace primec::ir_lowerer
