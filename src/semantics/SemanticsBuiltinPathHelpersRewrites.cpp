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

bool isExplicitRemovedCollectionCallAlias(std::string rawPath) {
  if (!rawPath.empty() && rawPath.front() == '/') {
    rawPath.erase(rawPath.begin());
  }

  std::string_view helperName;
  const std::string legacySoaPrefix = soa_paths::legacySoaFolder() + "/";
  if (rawPath.rfind(legacySoaPrefix, 0) == 0) {
    helperName = std::string_view(rawPath).substr(legacySoaPrefix.size());
    return !helperName.empty() && isRemovedBorrowedSoaCompatibilityHelper(helperName);
  }
  const std::string publicSoaPrefix = soa_paths::publicSoaFolder() + "/";
  if (rawPath.rfind(publicSoaPrefix, 0) == 0) {
    helperName = std::string_view(rawPath).substr(publicSoaPrefix.size());
    return !helperName.empty() && isRemovedBorrowedSoaCompatibilityHelper(helperName);
  }
  const std::string publicSoaStdPrefix =
      collectionPathPrefixLocal(soa_paths::publicSoaFolder());
  if (rawPath.rfind(publicSoaStdPrefix, 0) == 0) {
    helperName = std::string_view(rawPath).substr(publicSoaStdPrefix.size());
    return !helperName.empty() && isRemovedBorrowedSoaCompatibilityHelper(helperName);
  }
  if (rawPath.rfind("array/", 0) == 0) {
    helperName = std::string_view(rawPath).substr(std::string_view("array/").size());
    return !helperName.empty() && isRemovedVectorCompatibilityHelper(helperName);
  }
  std::string rootMapAliasHelperName;
  if (resolveRootMapAliasHelperMemberNameLocal(rawPath,
                                               rootMapAliasHelperName)) {
    helperName = rootMapAliasHelperName;
    return !helperName.empty() && isRemovedKeyValueCompatibilityHelper(helperName);
  }
  return false;
}

std::string removedRootMapMethodDiagnostic(const Expr &expr) {
  if (expr.kind != Expr::Kind::Call || !expr.isMethodCall ||
      expr.args.empty()) {
    return {};
  }
  std::string normalizedName = expr.name;
  if (!normalizedName.empty() && normalizedName.front() == '/') {
    normalizedName.erase(normalizedName.begin());
  }
  std::string normalizedPrefix = expr.namespacePrefix;
  if (!normalizedPrefix.empty() && normalizedPrefix.front() == '/') {
    normalizedPrefix.erase(normalizedPrefix.begin());
  }
  const std::string rootName = std::string("ma") + "p";
  const std::string rootPrefix = rootName + "/";
  std::string helperName;
  if (normalizedPrefix == rootName) {
    helperName = normalizedName;
  } else if (normalizedName.rfind(rootPrefix, 0) == 0) {
    helperName = normalizedName.substr(rootPrefix.size());
  } else {
    return {};
  }
  if (!isRemovedKeyValueCompatibilityHelper(helperName)) {
    return {};
  }
  return "unknown method: /" + rootPrefix + helperName;
}

bool isLifecycleHelperName(const std::string &fullPath) {
  static const std::array<HelperSuffixInfo, 10> suffixes = {{
      {"Create", ""},
      {"Destroy", ""},
      {"Copy", ""},
      {"Move", ""},
      {"CreateStack", "stack"},
      {"DestroyStack", "stack"},
      {"CreateHeap", "heap"},
      {"DestroyHeap", "heap"},
      {"CreateBuffer", "buffer"},
      {"DestroyBuffer", "buffer"},
  }};
  for (const auto &info : suffixes) {
    const std::string_view suffix = info.suffix;
    if (fullPath.size() < suffix.size() + 1) {
      continue;
    }
    const size_t suffixStart = fullPath.size() - suffix.size();
    if (fullPath[suffixStart - 1] != '/') {
      continue;
    }
    if (fullPath.compare(suffixStart, suffix.size(), suffix.data(), suffix.size()) != 0) {
      continue;
    }
    return true;
  }
  return false;
}

bool isMathBuiltinName(const std::string &name) {
  Expr probe;
  probe.name = name;
  std::string builtinName;
  return getBuiltinMathName(probe, builtinName, true) || getBuiltinClampName(probe, builtinName, true) ||
         getBuiltinMinMaxName(probe, builtinName, true) || getBuiltinAbsSignName(probe, builtinName, true) ||
         getBuiltinSaturateName(probe, builtinName, true) || isBuiltinMathConstant(name, true);
}

bool getBuiltinGpuName(const Expr &expr, std::string &out) {
  if (!parseGpuName(expr.name, out)) {
    return false;
  }
  return out == "global_id_x" || out == "global_id_y" || out == "global_id_z";
}

bool getBuiltinMemoryName(const Expr &expr, std::string &out) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  if (!parseMemoryName(resolveTypePath(expr.name, expr.namespacePrefix), out)) {
    return false;
  }
  return out == "alloc" || out == "free" || out == "realloc" || out == "at" || out == "at_unsafe" ||
         out == "reinterpret";
}

bool getBuiltinPointerName(const Expr &expr, std::string &out) {
  if (expr.name.empty()) {
    return false;
  }
  std::string name = expr.name;
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  const std::string legacyDereference =
      soa_paths::legacySoaFolder() + "/dereference";
  const std::string legacyLocation =
      soa_paths::legacySoaFolder() + "/location";
  if (name == "dereference" || name == "location" ||
      name == legacyDereference || name == legacyLocation) {
    if (name == legacyDereference) {
      out = "dereference";
      return true;
    }
    if (name == legacyLocation) {
      out = "location";
      return true;
    }
    out = name;
    return true;
  }
  if (name.find('/') != std::string::npos) {
    return false;
  }
  return false;
}

bool getBuiltinConvertName(const Expr &expr, std::string &out) {
  if (expr.name.empty()) {
    return false;
  }
  std::string name = expr.name;
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  if (name.find('/') != std::string::npos) {
    return false;
  }
  if (name == "convert") {
    out = name;
    return true;
  }
  return false;
}

bool getBuiltinCollectionName(const Expr &expr, std::string &out) {
  if (expr.name.empty()) {
    return false;
  }
  std::string name = expr.name;
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  if (name.rfind(collectionMemberRootLocal("vector"), 0) == 0) {
    return false;
  }
  std::string rootMapAliasHelperName;
  if (resolveRootMapAliasHelperMemberNameLocal(name,
                                               rootMapAliasHelperName)) {
    if (rootMapAliasHelperName == "map") {
      out = "map";
      return true;
    }
    return false;
  }
  std::string resolvedKeyValueHelperName;
  if (resolveKeyValueHelperMemberNameLocal(name, resolvedKeyValueHelperName)) {
    return false;
  }
  if (name.find('/') != std::string::npos) {
    return false;
  }
  if (name == "array" || name == "vector" || name == "map" || name == "soa") {
    out = name == "soa" ? internalSoaCollectionTypeName() : name;
    return true;
  }
  return false;
}

std::string soaFieldViewHelperPath(std::string_view fieldName) {
  return publicSoaHelperTargetPath("field_view") + "/" + std::string(fieldName);
}

bool splitSoaFieldViewHelperPath(std::string_view path, std::string *fieldNameOut) {
  const std::string publicSoaFieldViewPrefix =
      publicSoaHelperTargetPath("field_view") + "/";
  const std::string compatibilitySoaFieldViewPrefix =
      compatibilitySoaHelperTargetPath("field_view") + "/";
  auto splitWithPrefix = [&](const std::string &prefix) {
    if (fieldNameOut != nullptr) {
      *fieldNameOut = std::string(path.substr(prefix.size()));
    }
    return true;
  };
  if (path.starts_with(publicSoaFieldViewPrefix)) {
    return splitWithPrefix(publicSoaFieldViewPrefix);
  }
  if (path.starts_with(compatibilitySoaFieldViewPrefix)) {
    return splitWithPrefix(compatibilitySoaFieldViewPrefix);
  }
  return false;
}

bool isSoaFieldViewTypePath(std::string_view typeText) {
  std::string normalized = normalizeBindingTypeName(std::string(typeText));
  if (normalized.empty()) {
    return false;
  }
  std::string base;
  std::string arg;
  if (splitTemplateTypeName(normalized, base, arg)) {
    normalized = normalizeBindingTypeName(base);
  }
  if (!normalized.empty() && normalized.front() == '/') {
    normalized.erase(normalized.begin());
  }
  const size_t specializationSuffix = normalized.find("__");
  if (specializationSuffix != std::string::npos) {
    normalized.erase(specializationSuffix);
  }
  return normalized == "SoaFieldView" ||
         normalized == collection_paths::memberPathBare(
                           collection_paths::kInternalSoaStorageFolder, "SoaFieldView");
}

std::string canonicalizeLegacySoaToAosHelperPath(std::string_view path) {
  std::string canonicalPath = soa_paths::stripTemplateSpecializationSuffix(path);
  if (canonicalPath == "/to_aos") {
    return compatibilitySoaHelperTargetPath("to_aos");
  }
  if (canonicalPath == "/to_aos_ref") {
    return compatibilitySoaHelperTargetPath(collection_helpers::kToAosRef);
  }
  return canonicalPath;
}

std::string canonicalizeLegacySoaRefHelperPath(std::string_view path) {
  return soa_paths::canonicalizeLegacySoaRefHelperPath(path);
}

std::string canonicalizeLegacySoaGetHelperPath(std::string_view path) {
  return soa_paths::canonicalizeLegacySoaGetHelperPath(path);
}

bool isLegacyOrCanonicalSoaHelperPath(std::string_view path, std::string_view helperName) {
  return soa_paths::isLegacyOrCanonicalSoaHelperPath(path, helperName);
}

bool isCanonicalStdlibSoaHelperPath(std::string_view path, std::string_view helperName) {
  std::string canonicalPath(path);
  const size_t specializationSuffix = canonicalPath.find("__");
  if (specializationSuffix != std::string::npos) {
    canonicalPath.erase(specializationSuffix);
  }
  // TODO-5243: compatibilityPrefix/publicPrefix are compile-time constants
  // (they don't depend on `path`/`helperName`), but this function is
  // called from ~50 call sites across call/method resolution for every
  // candidate considered, including for compiles with zero SoA usage
  // where the answer is always false - profiling showed these two
  // rebuilt via string concatenation on every call. Precompute once.
  // TODO-5235: built via systemHeapValue() so these magic statics' backing
  // memory is never arena-allocated - see docs/CompilerArenaAllocator.md.
  static const std::string compatibilityPrefix = primec::systemHeapValue(
      [] { return compatibilitySoaHelperTargetPath("") + "/"; });
  static const std::string publicPrefix =
      primec::systemHeapValue([] { return publicSoaHelperTargetPath("") + "/"; });
  return (canonicalPath.rfind(compatibilityPrefix, 0) == 0 ||
          canonicalPath.rfind(publicPrefix, 0) == 0) &&
         isLegacyOrCanonicalSoaHelperPath(canonicalPath, helperName);
}

bool isCanonicalSoaRefLikeHelperPath(std::string_view path) {
  return soa_paths::isCanonicalSoaRefLikeHelperPath(path);
}

bool isExperimentalSoaCountLikeHelperPath(std::string_view path) {
  std::string canonicalPath(path);
  const size_t specializationSuffix = canonicalPath.find("__");
  if (specializationSuffix != std::string::npos) {
    canonicalPath.erase(specializationSuffix);
  }
  return canonicalPath == experimentalSoaHelperPathLocal("soaVectorCount") ||
         canonicalPath == experimentalSoaHelperPathLocal("soaVectorCountRef");
}

bool isExperimentalSoaBorrowedHelperPath(std::string_view path) {
  std::string canonicalPath(path);
  const size_t specializationSuffix = canonicalPath.find("__");
  if (specializationSuffix != std::string::npos) {
    canonicalPath.erase(specializationSuffix);
  }
  return canonicalPath == experimentalSoaHelperPathLocal("soaVectorCountRef") ||
         canonicalPath == experimentalSoaHelperPathLocal("soaVectorGetRef") ||
         canonicalPath == experimentalSoaHelperPathLocal("soaVectorRefRef");
}

bool isExperimentalSoaGetLikeHelperPath(std::string_view path) {
  std::string canonicalPath(path);
  const size_t specializationSuffix = canonicalPath.find("__");
  if (specializationSuffix != std::string::npos) {
    canonicalPath.erase(specializationSuffix);
  }
  return canonicalPath == experimentalSoaHelperPathLocal("soaVectorGet") ||
         canonicalPath == experimentalSoaHelperPathLocal("soaVectorGetRef");
}

bool isExperimentalSoaGrowthHelperPath(std::string_view path) {
  const std::string experimentalSoaPrefix =
      soa_paths::collectionPath(soa_paths::experimentalSoaFolder()) + "/";
  const std::string compatibilitySoaPrefix =
      compatibilitySoaHelperTargetPath("") + "/";
  std::string canonicalPath(path);
  const size_t specializationSuffix = canonicalPath.find("__");
  if (specializationSuffix != std::string::npos) {
    canonicalPath.erase(specializationSuffix);
  }
  if (canonicalPath.starts_with(compatibilitySoaPrefix)) {
    const std::string_view helperName =
        std::string_view(canonicalPath).substr(compatibilitySoaPrefix.size());
    return isStdlibSurfaceMemberName(StdlibSurfaceId::CollectionsColumnarHelpers, helperName) ||
           helperName == "soaVectorPush" ||
           helperName == "soaVectorReserve";
  }
  if (canonicalPath.starts_with(experimentalSoaPrefix)) {
    const std::string_view helperName =
        std::string_view(canonicalPath).substr(experimentalSoaPrefix.size());
    return isStdlibSurfaceMemberName(StdlibSurfaceId::CollectionsColumnarHelpers, helperName) ||
           helperName == "soaVectorPush" ||
           helperName == "soaVectorReserve";
  }
  return false;
}

} // namespace primec::semantics
