#include "SemanticsHelpers.h"

#include "StdlibCollectionSurfaceHelpers.h"
#include "primec/support/BuiltinArrayAccessNameClassifier.h"
#include "primec/support/CollectionSpellingClassifier.h"
#include "primec/support/CompileArena.h"
#include "primec/ir/SoaPathHelpers.h"
#include "primec/ir/StdlibCollectionPaths.h"
#include "primec/support/StdlibSurfaceRegistry.h"
#include "primec/support/CollectionHelperNames.h"

#include <array>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string_view>
#include <utility>
#include "SemanticsBuiltinPathHelpersShared.h"

namespace primec::semantics {
using namespace builtinPathHelpers;

bool isExperimentalSoaFieldViewHelperPath(std::string_view path) {
  const std::string experimentalSoaFieldViewPrefix =
      experimentalSoaHelperPathLocal("soaVectorFieldView");
  const std::string canonicalSoaFieldViewPrefix =
      compatibilitySoaHelperTargetPath("soaVectorFieldView");
  const std::string publicSoaFieldViewPath =
      publicSoaHelperTargetPath("field_view");
  const std::string kExperimentalSoaColumnFieldViewUnsafePrefix =
      collection_paths::memberPath(collection_paths::kInternalSoaStorageFolder,
                                   "soaColumnFieldViewUnsafe");
  std::string canonicalPath(path);
  const size_t specializationSuffix = canonicalPath.find("__");
  if (specializationSuffix != std::string::npos) {
    canonicalPath.erase(specializationSuffix);
  }
  return canonicalPath.rfind(experimentalSoaFieldViewPrefix, 0) == 0 ||
         canonicalPath.rfind(canonicalSoaFieldViewPrefix, 0) == 0 ||
         canonicalPath == publicSoaFieldViewPath ||
         canonicalPath.rfind(kExperimentalSoaColumnFieldViewUnsafePrefix, 0) == 0;
}

bool isExperimentalSoaFieldViewReadHelperPath(std::string_view path) {
  std::string canonicalPath(path);
  const size_t specializationSuffix = canonicalPath.find("__");
  if (specializationSuffix != std::string::npos) {
    canonicalPath.erase(specializationSuffix);
  }
  return canonicalPath.rfind(
             collection_paths::memberPath(collection_paths::kInternalSoaStorageFolder,
                                          "soaFieldViewRead"),
             0) == 0;
}

bool isExperimentalSoaFieldViewRefHelperPath(std::string_view path) {
  std::string canonicalPath(path);
  const size_t specializationSuffix = canonicalPath.find("__");
  if (specializationSuffix != std::string::npos) {
    canonicalPath.erase(specializationSuffix);
  }
  return canonicalPath == collection_paths::memberPath(
                              collection_paths::kInternalSoaStorageFolder, "soaFieldViewRef");
}

bool isExperimentalSoaColumnSlotHelperPath(std::string_view path) {
  std::string canonicalPath(path);
  const size_t specializationSuffix = canonicalPath.find("__");
  if (specializationSuffix != std::string::npos) {
    canonicalPath.erase(specializationSuffix);
  }
  return canonicalPath.rfind(
             collection_paths::memberPath(collection_paths::kInternalSoaStorageFolder,
                                          "soaColumnSlotUnsafe"),
             0) == 0;
}

bool isExperimentalSoaColumnFieldSchemaHelperPath(std::string_view path) {
  std::string canonicalPath(path);
  const size_t specializationSuffix = canonicalPath.find("__");
  if (specializationSuffix != std::string::npos) {
    canonicalPath.erase(specializationSuffix);
  }
  return canonicalPath.rfind(
             collection_paths::memberPath(collection_paths::kInternalSoaStorageFolder,
                                          "soaColumnField"),
             0) == 0;
}

bool isExperimentalSoaMethodRefLikeHelperPath(std::string_view path) {
  const std::string experimentalSoaPrefix =
      soa_paths::collectionPath(soa_paths::experimentalSoaFolder()) + "/";
  std::string canonicalPath(path);
  const size_t specializationSuffix = canonicalPath.find("__");
  if (specializationSuffix != std::string::npos) {
    canonicalPath.erase(specializationSuffix);
  }
  if (canonicalPath.rfind(experimentalSoaPrefix, 0) != 0) {
    return false;
  }
  return std::string_view(canonicalPath).ends_with("/ref") ||
         std::string_view(canonicalPath).ends_with("/ref_ref");
}

bool isExperimentalSoaRefLikeHelperPath(std::string_view path) {
  std::string canonicalPath(path);
  const size_t specializationSuffix = canonicalPath.find("__");
  if (specializationSuffix != std::string::npos) {
    canonicalPath.erase(specializationSuffix);
  }
  return canonicalPath == experimentalSoaHelperPathLocal("soaVectorRef") ||
         canonicalPath == experimentalSoaHelperPathLocal("soaVectorRefRef") ||
         canonicalPath == collection_paths::memberPath(
                              collection_paths::kInternalSoaStorageFolder, "soaColumnRef");
}

bool isExperimentalSoaVectorHelperFamilyPath(std::string_view path) {
  const std::string experimentalSoaPrefix =
      soa_paths::collectionPath(soa_paths::experimentalSoaFolder()) + "/";
  std::string canonicalPath(path);
  const size_t specializationSuffix = canonicalPath.find("__");
  if (specializationSuffix != std::string::npos) {
    canonicalPath.erase(specializationSuffix);
  }
  return canonicalPath.starts_with(experimentalSoaPrefix) ||
         isExperimentalSoaVectorConversionFamilyPath(canonicalPath);
}

bool isExperimentalSoaVectorTypePath(std::string_view path) {
  return soa_paths::isExperimentalColumnarVectorTypePath(path);
}

bool isExperimentalSoaVectorSpecializedTypePath(std::string_view path) {
  return soa_paths::isExperimentalColumnarVectorSpecializedTypePath(path);
}

bool isExperimentalSoaVectorConversionFamilyPath(std::string_view path) {
  const std::string experimentalSoaConversionsPrefix =
      soa_paths::collectionPath(
          soa_paths::experimentalSoaFolder() + "_conversions") +
      "/";
  std::string canonicalPath(path);
  const size_t specializationSuffix = canonicalPath.find("__");
  if (specializationSuffix != std::string::npos) {
    canonicalPath.erase(specializationSuffix);
  }
  if (!canonicalPath.starts_with(experimentalSoaConversionsPrefix)) {
    return false;
  }
  const std::string_view helperName =
      std::string_view(canonicalPath).substr(experimentalSoaConversionsPrefix.size());
  return helperName == "soaVectorToAos" ||
         helperName == "soaVectorToAosRef";
}

namespace {

std::string canonicalSoaPendingHelperPath(std::string_view resolvedPath) {
  std::string resolvedPathNoTemplate;
  std::string_view normalizedResolvedPath = resolvedPath;
  const size_t templateSuffix = resolvedPath.find("__t");
  if (templateSuffix != std::string_view::npos) {
    resolvedPathNoTemplate = std::string(resolvedPath.substr(0, templateSuffix));
    normalizedResolvedPath = resolvedPathNoTemplate;
  }
  std::string fieldName;
  if (splitSoaFieldViewHelperPath(normalizedResolvedPath, &fieldName)) {
    return soaFieldViewHelperPath(fieldName);
  }
  const std::string canonicalSoaRefPath =
      canonicalizeLegacySoaRefHelperPath(normalizedResolvedPath);
  if (isCanonicalSoaRefLikeHelperPath(canonicalSoaRefPath)) {
    return canonicalSoaRefPath;
  }
  const std::string canonicalSoaGetPath =
      canonicalizeLegacySoaGetHelperPath(normalizedResolvedPath);
  if (isLegacyOrCanonicalSoaHelperPath(canonicalSoaGetPath, "get") ||
      isLegacyOrCanonicalSoaHelperPath(canonicalSoaGetPath, collection_helpers::kGetRef)) {
    return canonicalSoaGetPath;
  }
  // TODO-5319: `/soa/count` and `/soa/count_ref` used to be reported under
  // the retired /std/collections/soa_vector/* family here. They now stay
  // on their own `/soa/<helper>` spelling like to_aos/push/reserve.
  return std::string(resolvedPath);
}

} // namespace

std::string soaUnavailableMethodDiagnostic(std::string_view resolvedPath) {
  return "unknown method: " + canonicalSoaPendingHelperPath(resolvedPath);
}

namespace {

// TODO-5293 Step (6) (docs/ReceiverTargetResolutionConsolidation.md):
// production key-value-helper lookup callback for the shared
// primec::BuiltinArrayAccessNameClassifier composition below. Reproduces
// semantics' real branch-5 shape (resolveKeyValueHelperMemberNameLocal,
// including its surface-metadata-id cross-check, then classified like
// every other branch) - previously used only by the now-retired
// diff-audit harness (TODO-5293 Step (4)), now the real production
// delegate.
primec::BuiltinArrayAccessAliasResult semanticsKeyValueLookup(std::string_view name) {
  std::string keyValueHelperName;
  if (!resolveKeyValueHelperMemberNameLocal(std::string(name), keyValueHelperName)) {
    return {};
  }
  std::optional<std::string> token =
      primec::classifyAccessAliasToken(keyValueHelperName, primec::AccessAliasSpellingMode::kFull);
  if (token) {
    return primec::BuiltinArrayAccessAliasResult{primec::BuiltinArrayAccessAliasOutcome::kAccept, *token};
  }
  return primec::BuiltinArrayAccessAliasResult{primec::BuiltinArrayAccessAliasOutcome::kReject, {}};
}

}  // namespace

// TODO-5293 Step (6): migrated onto the shared
// primec::BuiltinArrayAccessNameClassifier module (classifyBuiltinArray-
// AccessNameForSemantics) after 6 rounds of characterization, unit-testing,
// and dynamic zero-divergence proof against real 3-suite test traffic via
// the (now-retired) PRIMESTRUCT_RECEIVER_TARGET_DIFF_AUDIT harness (Step
// (4)). This function's own inline logic previously duplicated the
// classifier's `classifyBuiltinArrayAccessNameForSemantics` composition
// bit-for-bit; the two are no longer separately maintained.
bool getBuiltinArrayAccessName(const Expr &expr, std::string &out) {
  if (expr.name.empty()) {
    return false;
  }
  std::string name = expr.name;
  if (!expr.namespacePrefix.empty() && name.find('/') == std::string::npos) {
    std::string prefix = expr.namespacePrefix;
    if (!prefix.empty() && prefix.front() == '/') {
      prefix.erase(prefix.begin());
    }
    name = prefix.empty() ? name : prefix + "/" + name;
  }
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  std::string rawName = expr.name;
  if (!rawName.empty() && rawName[0] == '/') {
    rawName.erase(0, 1);
  }
  return primec::classifyBuiltinArrayAccessNameForSemantics(
      name, rawName, "std/collections/", experimentalCollectionMemberRootLocal("vector"),
      experimentalCollectionMemberRootLocal("map"), collectionMemberRootLocal("vector"),
      semanticsKeyValueLookup, out);
}

bool getNamespacedCollectionHelperName(const Expr &expr, std::string &collectionOut, std::string &helperOut) {
  collectionOut.clear();
  helperOut.clear();
  if (expr.name.empty()) {
    return false;
  }
  auto stripTemplateSpecializationSuffix = [](std::string value) {
    const size_t suffix = value.find("__t");
    if (suffix != std::string::npos) {
      value.erase(suffix);
    }
    return value;
  };
  std::string normalized = expr.name;
  if (!expr.namespacePrefix.empty() && normalized.find('/') == std::string::npos) {
    std::string prefix = expr.namespacePrefix;
    if (!prefix.empty() && prefix.front() == '/') {
      prefix.erase(prefix.begin());
    }
    if (!prefix.empty()) {
      normalized = prefix + "/" + normalized;
    }
  }
  if (!normalized.empty() && normalized.front() == '/') {
    normalized.erase(normalized.begin());
  }

  auto extractHelper = [&](const std::string &prefix, const std::string &collectionName) -> bool {
    if (normalized.rfind(prefix, 0) != 0) {
      return false;
    }
    collectionOut = collectionName;
    helperOut = stripTemplateSpecializationSuffix(normalized.substr(prefix.size()));
    return !helperOut.empty();
  };

  if (extractHelper(collectionMemberRootLocal("vector"), "vector")) {
    return true;
  }
  std::string keyValueHelperName;
  if (resolveKeyValueHelperMemberNameLocal(normalized, keyValueHelperName)) {
    collectionOut = "map";
    helperOut = keyValueHelperName;
    return true;
  }
  if (extractHelper("array/", "vector")) {
    if (helperOut == "count" || helperOut == "capacity" || helperOut == "at" || helperOut == "at_unsafe" ||
        helperOut == "push" || helperOut == "pop" || helperOut == "reserve" || helperOut == "clear" ||
        helperOut == "remove_at" || helperOut == "remove_swap") {
      collectionOut.clear();
      helperOut.clear();
      return false;
    }
    return true;
  }
  collectionOut.clear();
  helperOut.clear();
  return false;
}

} // namespace primec::semantics
