#include "SemanticsHelpers.h"

#include "primec/SoaPathHelpers.h"
#include "primec/StdlibSurfaceRegistry.h"

#include <array>
#include <string_view>
#include <utility>

namespace primec::semantics {

namespace {

bool isRemovedVectorCompatibilityHelper(std::string_view helperName) {
  return helperName == "count" || helperName == "capacity" || helperName == "at" || helperName == "at_unsafe" ||
         helperName == "push" || helperName == "pop" || helperName == "reserve" || helperName == "clear" ||
         helperName == "remove_at" || helperName == "remove_swap";
}

bool isRemovedBorrowedSoaCompatibilityHelper(std::string_view helperName) {
  return helperName == "count_ref" || helperName == "get_ref" ||
         helperName == "ref_ref" || helperName == "to" "_aos_ref";
}

bool isRemovedKeyValueCompatibilityHelper(std::string_view helperName) {
  return helperName == "count" || helperName == "count_ref" ||
         helperName == "size" ||
         helperName == "contains" || helperName == "contains_ref" ||
         helperName == "tryAt" || helperName == "tryAt_ref" ||
         helperName == "at" || helperName == "at_ref" ||
         helperName == "at_unsafe" || helperName == "at_unsafe_ref" ||
         helperName == "insert" || helperName == "insert_ref";
}

struct HelperSuffixInfo {
  std::string_view suffix;
  std::string_view placement;
};

bool parseMathName(const std::string &name, std::string &out, bool allowBare) {
  if (name.empty()) {
    return false;
  }
  std::string normalized = name;
  const bool hasLeadingSlash = !normalized.empty() && normalized[0] == '/';
  if (hasLeadingSlash) {
    normalized.erase(0, 1);
  }
  if (normalized.rfind("std/math/", 0) == 0) {
    out = normalized.substr(9);
    return true;
  }
  if (normalized.find('/') != std::string::npos) {
    return false;
  }
  if (hasLeadingSlash) {
    out = normalized;
    return true;
  }
  if (!allowBare) {
    return false;
  }
  out = normalized;
  return true;
}

bool parseGpuName(const std::string &name, std::string &out) {
  if (name.empty()) {
    return false;
  }
  std::string normalized = name;
  if (!normalized.empty() && normalized[0] == '/') {
    normalized.erase(0, 1);
  }
  if (normalized.rfind("std/gpu/", 0) == 0) {
    out = normalized.substr(8);
    return true;
  }
  if (normalized.find('/') != std::string::npos) {
    return false;
  }
  return false;
}

bool parseMemoryName(const std::string &name, std::string &out) {
  if (name.empty()) {
    return false;
  }
  std::string normalized = name;
  if (!normalized.empty() && normalized[0] == '/') {
    normalized.erase(0, 1);
  }
  if (normalized.rfind("std/intrinsics/memory/", 0) == 0) {
    out = normalized.substr(22);
    return true;
  }
  return false;
}

std::string collectionMemberRootLocal(std::string_view collectionName,
                                      bool leadingSlash = false) {
  std::string root = leadingSlash ? "/" : "";
  root += "std/collections/";
  root += std::string(collectionName);
  root += "/";
  return root;
}

std::string experimentalCollectionMemberRootLocal(
    std::string_view collectionName,
    bool leadingSlash = false) {
  std::string root = leadingSlash ? "/" : "";
  root += "std/collections/experimental_";
  root += std::string(collectionName);
  root += "/";
  return root;
}

std::string collectionAliasLocal(std::string_view collectionName,
                                 std::string_view suffix) {
  return std::string(collectionName) + std::string(suffix);
}

std::string collectionNamespaceLocal(std::string_view collectionName) {
  std::string namespacePath = collectionMemberRootLocal(collectionName);
  if (!namespacePath.empty() && namespacePath.back() == '/') {
    namespacePath.pop_back();
  }
  return namespacePath;
}

bool typePathMatchesLocal(std::string_view normalizedTypePath,
                          std::string_view bareName,
                          std::string_view fullPathNoSlash) {
  if (!normalizedTypePath.empty() && normalizedTypePath.front() == '/') {
    normalizedTypePath.remove_prefix(1);
  }
  const std::string bareTemplatePrefix = std::string(bareName) + "<";
  const std::string fullTemplatePrefix = std::string(fullPathNoSlash) + "<";
  const std::string bareSpecializedPrefix = std::string(bareName) + "__";
  const std::string fullSpecializedPrefix = std::string(fullPathNoSlash) + "__";
  auto startsWith = [](std::string_view value, const std::string &prefix) {
    return value.starts_with(std::string_view(prefix));
  };
  return normalizedTypePath == bareName ||
         normalizedTypePath == fullPathNoSlash ||
         startsWith(normalizedTypePath, bareTemplatePrefix) ||
         startsWith(normalizedTypePath, fullTemplatePrefix) ||
         startsWith(normalizedTypePath, bareSpecializedPrefix) ||
         startsWith(normalizedTypePath, fullSpecializedPrefix);
}

std::string pathWithoutLeadingSlash(std::string path) {
  if (!path.empty() && path.front() == '/') {
    path.erase(path.begin());
  }
  return path;
}

const primec::StdlibSurfaceMetadata *keyValueHelperSurfaceMetadataLocal() {
  return primec::findStdlibSurfaceMetadataByBridgeKey("collections.map_helpers");
}

bool stripStdlibSurfaceRootedMemberNameLocal(std::string_view rawPath,
                                             std::string_view rawRoot,
                                             std::string &memberNameOut) {
  memberNameOut.clear();
  if (rawPath.empty() || rawRoot.empty()) {
    return false;
  }
  std::string path(rawPath);
  if (!path.empty() && path.front() == '/') {
    path.erase(path.begin());
  }
  std::string root(rawRoot);
  if (!root.empty() && root.front() == '/') {
    root.erase(root.begin());
  }
  if (path.size() <= root.size() || path.rfind(root, 0) != 0 ||
      path[root.size()] != '/') {
    return false;
  }
  std::string memberName = path.substr(root.size() + 1);
  if (memberName.empty() || memberName.find('/') != std::string::npos) {
    return false;
  }
  memberNameOut = std::move(memberName);
  return true;
}

bool resolveKeyValueHelperMemberNameLocal(std::string rawPath,
                                          std::string &memberNameOut) {
  memberNameOut.clear();
  const primec::StdlibSurfaceMetadata *metadata =
      keyValueHelperSurfaceMetadataLocal();
  if (metadata == nullptr || rawPath.empty()) {
    return false;
  }
  if (rawPath.find('/') == std::string::npos) {
    return false;
  }
  if (rawPath.find('/') != std::string::npos && rawPath.front() != '/') {
    rawPath.insert(rawPath.begin(), '/');
  }
  if (rawPath.find('/') != std::string::npos) {
    const primec::StdlibSurfaceMetadata *pathMetadata =
        primec::findStdlibSurfaceMetadataByResolvedPath(rawPath);
    if (pathMetadata == nullptr || pathMetadata->id != metadata->id) {
      return false;
    }
  }
  const std::string_view memberName =
      primec::resolveStdlibSurfaceMemberName(*metadata, rawPath);
  if (memberName.empty()) {
    return false;
  }
  memberNameOut.assign(memberName);
  return true;
}

bool resolveRootMapAliasHelperMemberNameLocal(std::string_view rawPath,
                                              std::string &memberNameOut) {
  memberNameOut.clear();
  const primec::StdlibSurfaceMetadata *metadata =
      keyValueHelperSurfaceMetadataLocal();
  if (metadata == nullptr) {
    return false;
  }
  for (std::string_view alias : metadata->importAliasSpellings) {
    if (!alias.empty() && alias.front() == '/') {
      alias.remove_prefix(1);
    }
    if (alias.find('/') != std::string_view::npos) {
      continue;
    }
    if (stripStdlibSurfaceRootedMemberNameLocal(rawPath, alias,
                                                memberNameOut)) {
      return true;
    }
  }
  return false;
}

std::string collectionPathPrefixLocal(std::string_view collectionName) {
  return pathWithoutLeadingSlash(
             soa_paths::collectionPath(collectionName)) +
         "/";
}

std::string experimentalSoaHelperPathLocal(std::string_view helperName) {
  return soa_paths::collectionPath(soa_paths::experimentalSoaFolder(),
                                   helperName);
}

} // namespace

std::string samePathSoaHelperTargetPath(std::string_view helperName) {
  if (helperName == "to" "_aos" || helperName == "to" "_aos_ref") {
    return "/" + std::string(helperName);
  }
  return "/" + soa_paths::legacySoaFolder() + "/" + std::string(helperName);
}

std::string publicSoaHelperTargetPath(std::string_view helperName) {
  return soa_paths::collectionPath(soa_paths::publicSoaFolder(), helperName);
}

std::string compatibilitySoaHelperTargetPath(std::string_view helperName) {
  return soa_paths::collectionPath(soa_paths::legacySoaFolder(), helperName);
}

bool splitSoaSurfaceHelperPath(std::string_view path,
                               std::string *helperNameOut,
                               bool *usesPublicSurfaceOut) {
  if (!path.empty() && path.front() == '/') {
    path.remove_prefix(1);
  }
  const std::string publicPrefix = soa_paths::publicSoaFolder() + "/";
  const std::string publicStdPrefix =
      collectionPathPrefixLocal(soa_paths::publicSoaFolder());
  const std::string compatibilityPrefix =
      soa_paths::legacySoaFolder() + "/";
  const std::string compatibilityStdPrefix =
      collectionPathPrefixLocal(soa_paths::legacySoaFolder());
  auto splitWithPrefix = [&](std::string_view prefix, bool usesPublicSurface) {
    if (!path.starts_with(prefix)) {
      return false;
    }
    if (helperNameOut != nullptr) {
      *helperNameOut = std::string(path.substr(prefix.size()));
    }
    if (usesPublicSurfaceOut != nullptr) {
      *usesPublicSurfaceOut = usesPublicSurface;
    }
    return true;
  };
  return splitWithPrefix(publicPrefix, true) ||
         splitWithPrefix(publicStdPrefix, true) ||
         splitWithPrefix(compatibilityPrefix, false) ||
         splitWithPrefix(compatibilityStdPrefix, false);
}

bool isSoaReadRefHelperName(std::string_view helperName) {
  return helperName == "count" || helperName == "count_ref" ||
         helperName == "get" || helperName == "get_ref" ||
         helperName == "ref" || helperName == "ref_ref";
}

bool isExplicitPublicSoaSurfaceHelperName(std::string_view helperName) {
  return isSoaReadRefHelperName(helperName) ||
         helperName == "soa" || helperName == "single" ||
         helperName == "from" "_aos" || helperName == "field_view";
}

bool isSupportedCompatibilitySoaHelperName(std::string_view helperName) {
  return isSoaReadRefHelperName(helperName) ||
         helperName == "to" "_aos" || helperName == "to" "_aos_ref" ||
         helperName == "push" || helperName == "reserve";
}

bool isPublicSoaSurfaceNamespace(std::string_view namespacePath) {
  if (!namespacePath.empty() && namespacePath.front() == '/') {
    namespacePath.remove_prefix(1);
  }
  return namespacePath == "soa" ||
         namespacePath == collectionNamespaceLocal("soa");
}

bool isCompatibilitySoaSurfaceNamespace(std::string_view namespacePath) {
  if (!namespacePath.empty() && namespacePath.front() == '/') {
    namespacePath.remove_prefix(1);
  }
  return namespacePath == soa_paths::legacySoaFolder() ||
         namespacePath == pathWithoutLeadingSlash(
                              soa_paths::collectionPath(
                                  soa_paths::legacySoaFolder()));
}

bool isSoaConversionSurfaceSpelling(std::string_view normalizedPrefix,
                                    std::string_view normalizedName) {
  std::string helperName;
  bool usesPublicSurface = false;
  if (splitSoaSurfaceHelperPath(normalizedName, &helperName, &usesPublicSurface)) {
    return helperName == "to" "_aos" || helperName == "to" "_aos_ref" ||
           (!usesPublicSurface && helperName == "to_soa");
  }
  if (isPublicSoaSurfaceNamespace(normalizedPrefix) &&
      normalizedName == "to" "_aos") {
    return true;
  }
  if (isCompatibilitySoaSurfaceNamespace(normalizedPrefix) &&
      (normalizedName == "to_soa" || normalizedName == "to" "_aos" ||
       normalizedName == "to" "_aos_ref")) {
    return true;
  }
  return normalizedName == "to_soa" || normalizedName == "to" "_aos" ||
         normalizedName == "to" "_aos_ref";
}

bool isSoaCountOrAccessSurfaceSpelling(std::string_view normalizedPrefix,
                                       std::string_view normalizedName) {
  std::string helperName;
  if (splitSoaSurfaceHelperPath(normalizedName, &helperName, nullptr)) {
    return isSoaReadRefHelperName(helperName);
  }
  if ((isCompatibilitySoaSurfaceNamespace(normalizedPrefix) ||
       isPublicSoaSurfaceNamespace(normalizedPrefix)) &&
      isSoaReadRefHelperName(normalizedName)) {
    return true;
  }
  return isSoaReadRefHelperName(normalizedName);
}

bool usesExplicitPublicSoaHelperPath(std::string_view normalizedPrefix,
                                     std::string_view normalizedName) {
  bool usesPublicSurface = false;
  std::string ignoredHelperName;
  if (splitSoaSurfaceHelperPath(normalizedName,
                                &ignoredHelperName,
                                &usesPublicSurface)) {
    return usesPublicSurface;
  }
  return isPublicSoaSurfaceNamespace(normalizedPrefix);
}

std::string internalSoaCollectionTypeName() {
  return soa_paths::legacySoaFolder();
}

std::string internalSoaCollectionTypePath(bool leadingSlash) {
  std::string path = collectionNamespaceLocal(internalSoaCollectionTypeName());
  if (leadingSlash) {
    path.insert(path.begin(), '/');
  }
  return path;
}

std::string experimentalSoaStorageTypeName() {
  return soa_paths::soaBackingTypeName();
}

std::string experimentalSoaStorageTypePath(bool leadingSlash) {
  std::string path = experimentalCollectionMemberRootLocal(
      internalSoaCollectionTypeName());
  path += experimentalSoaStorageTypeName();
  if (leadingSlash) {
    path.insert(path.begin(), '/');
  }
  return path;
}

bool isInternalSoaCollectionTypeName(std::string_view normalizedTypeName) {
  return typePathMatchesLocal(normalizedTypeName,
                              internalSoaCollectionTypeName(),
                              internalSoaCollectionTypePath(false));
}

bool isInternalSoaCollectionTypePath(std::string_view normalizedTypePath) {
  return typePathMatchesLocal(normalizedTypePath,
                              internalSoaCollectionTypeName(),
                              internalSoaCollectionTypePath(false));
}

bool isInternalOrExperimentalSoaStorageTypePath(std::string_view normalizedTypePath) {
  return isInternalSoaCollectionTypePath(normalizedTypePath) ||
         isExperimentalSoaVectorTypePath(normalizedTypePath);
}

bool getBuiltinOperatorName(const Expr &expr, std::string &out) {
  if (expr.name.empty()) {
    return false;
  }
  std::string name = expr.name;
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  if (name.rfind("std/gpu/", 0) == 0) {
    name.erase(0, 8);
  }
  if (name.find('/') != std::string::npos) {
    return false;
  }
  if (name == "plus" || name == "minus" || name == "multiply" || name == "divide" || name == "negate") {
    out = name;
    return true;
  }
  return false;
}

bool getBuiltinComparisonName(const Expr &expr, std::string &out) {
  if (expr.name.empty()) {
    return false;
  }
  std::string name = expr.name;
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  if (name.rfind("std/gpu/", 0) == 0) {
    name.erase(0, 8);
  }
  if (name.find('/') != std::string::npos) {
    return false;
  }
  if (name == "greater_than" || name == "less_than" || name == "equal" || name == "not_equal" ||
      name == "greater_equal" || name == "less_equal" || name == "and" || name == "or" || name == "not") {
    out = name;
    return true;
  }
  return false;
}

bool getBuiltinMutationName(const Expr &expr, std::string &out) {
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
  if (name == "increment" || name == "decrement") {
    out = name;
    return true;
  }
  return false;
}

bool isRootBuiltinName(const std::string &name) {
  if (name.empty()) {
    return false;
  }
  std::string normalized = name;
  if (!normalized.empty() && normalized[0] == '/') {
    normalized.erase(0, 1);
  }
  std::string gpuName;
  if (parseGpuName(normalized, gpuName)) {
    return gpuName == "global_id_x" || gpuName == "global_id_y" || gpuName == "global_id_z";
  }
  std::string memoryName;
  if (parseMemoryName(normalized, memoryName)) {
    return memoryName == "alloc" || memoryName == "free" || memoryName == "realloc";
  }
  bool isStdGpuQualified = false;
  if (normalized.rfind("std/gpu/", 0) == 0) {
    normalized = normalized.substr(8);
    isStdGpuQualified = true;
  }
  if (normalized.find('/') != std::string::npos) {
    return false;
  }
  Expr probe;
  probe.name = normalized;
  std::string builtinName;
  if (getBuiltinOperatorName(probe, builtinName) || getBuiltinComparisonName(probe, builtinName)) {
    return true;
  }
  if (normalized == "increment" || normalized == "decrement") {
    return true;
  }
  return normalized == "assign" || normalized == "move" || normalized == "if" || normalized == "then" ||
         normalized == "else" ||
         normalized == "loop" || normalized == "while" || normalized == "for" || normalized == "repeat" ||
         normalized == "return" || normalized == "array" || normalized == "vector" ||
         normalized == "map" || normalized == "Task" || normalized == "File" ||
         normalized == "try" || normalized == "count" || normalized == "capacity" ||
         normalized == "to_soa" || normalized == "to" "_aos" ||
         normalized == "to" "_aos_ref" ||
         normalized == "push" || normalized == "pop" ||
         normalized == "reserve" || normalized == "clear" || normalized == "remove_at" || normalized == "remove_swap" ||
         normalized == "at" || normalized == "at_unsafe" || normalized == "convert" ||
         normalized == "location" || normalized == "dereference" ||
         normalized == "block" || normalized == "print" || normalized == "print_line" ||
         normalized == "print_error" || normalized == "print_line_error" || normalized == "notify" ||
         normalized == "insert" || normalized == "take" ||
         (isStdGpuQualified &&
          (normalized == "dispatch" || normalized == "buffer" || normalized == "upload" || normalized == "readback" ||
           normalized == "buffer_load" || normalized == "buffer_store"));
}

bool getBuiltinClampName(const Expr &expr, std::string &out, bool allowBare) {
  if (!parseMathName(expr.name, out, allowBare)) {
    return false;
  }
  return out == "clamp";
}

bool getBuiltinMinMaxName(const Expr &expr, std::string &out, bool allowBare) {
  if (!parseMathName(expr.name, out, allowBare)) {
    return false;
  }
  return out == "min" || out == "max";
}

bool getBuiltinAbsSignName(const Expr &expr, std::string &out, bool allowBare) {
  if (!parseMathName(expr.name, out, allowBare)) {
    return false;
  }
  return out == "abs" || out == "sign";
}

bool getBuiltinSaturateName(const Expr &expr, std::string &out, bool allowBare) {
  if (!parseMathName(expr.name, out, allowBare)) {
    return false;
  }
  return out == "saturate";
}

bool getBuiltinMathName(const Expr &expr, std::string &out, bool allowBare) {
  if (!parseMathName(expr.name, out, allowBare)) {
    return false;
  }
  if (out == "lerp" || out == "floor" || out == "ceil" || out == "round" || out == "trunc" || out == "fract" ||
      out == "sqrt" || out == "cbrt" || out == "pow" || out == "exp" || out == "exp2" || out == "log" ||
      out == "log2" || out == "log10" || out == "sin" || out == "cos" || out == "tan" || out == "asin" ||
      out == "acos" || out == "atan" || out == "atan2" || out == "radians" || out == "degrees" || out == "sinh" ||
      out == "cosh" || out == "tanh" || out == "asinh" || out == "acosh" || out == "atanh" || out == "fma" ||
      out == "hypot" || out == "copysign" || out == "is_nan" || out == "is_inf" || out == "is_finite") {
    return true;
  }
  return false;
}

bool isBuiltinMathConstant(const std::string &name, bool allowBare) {
  std::string candidate;
  if (!parseMathName(name, candidate, allowBare)) {
    return false;
  }
  return candidate == "pi" || candidate == "tau" || candidate == "e";
}

bool isExplicitRemovedCollectionMethodAlias(const std::string &receiverPath, std::string rawMethodName) {
  if (!rawMethodName.empty() && rawMethodName.front() == '/') {
    rawMethodName.erase(rawMethodName.begin());
  }

  std::string_view helperName;
  const bool isSoaVectorReceiver =
      receiverPath == "/" + soa_paths::legacySoaFolder() ||
      receiverPath == compatibilitySoaHelperTargetPath("");
  if (isSoaVectorReceiver) {
    const std::string aliasPrefix = soa_paths::legacySoaFolder() + "/";
    const std::string stdPrefix =
        collectionPathPrefixLocal(soa_paths::legacySoaFolder());
    if (rawMethodName.rfind(aliasPrefix, 0) == 0) {
      helperName = std::string_view(rawMethodName).substr(aliasPrefix.size());
    } else if (rawMethodName.rfind(stdPrefix, 0) == 0) {
      helperName =
          std::string_view(rawMethodName).substr(stdPrefix.size());
    }
    return !helperName.empty() && isRemovedBorrowedSoaCompatibilityHelper(helperName);
  }

  const bool isVectorFamilyReceiver = receiverPath == "/array" || receiverPath == "/vector";
  if (isVectorFamilyReceiver) {
    if (rawMethodName.rfind("array/", 0) == 0) {
      helperName = std::string_view(rawMethodName).substr(std::string_view("array/").size());
    } else {
      const std::string stdVectorRoot = collectionMemberRootLocal("vector");
      if (rawMethodName.rfind(stdVectorRoot, 0) == 0) {
        helperName = std::string_view(rawMethodName).substr(stdVectorRoot.size());
      }
    }
    return !helperName.empty() && isRemovedVectorCompatibilityHelper(helperName);
  }

  if (receiverPath != "/map") {
    return false;
  }
  std::string resolvedKeyValueHelperName;
  if (resolveKeyValueHelperMemberNameLocal(rawMethodName,
                                           resolvedKeyValueHelperName)) {
    helperName = resolvedKeyValueHelperName;
  }
  return !helperName.empty() && isRemovedKeyValueCompatibilityHelper(helperName);
}

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
         normalized == "std/collections/internal_soa_storage/SoaFieldView";
}

std::string canonicalizeLegacySoaToAosHelperPath(std::string_view path) {
  std::string canonicalPath(path);
  const size_t templateSuffix = canonicalPath.find("__t");
  if (templateSuffix != std::string::npos) {
    canonicalPath.erase(templateSuffix);
  }
  if (canonicalPath == "/to" "_aos") {
    return compatibilitySoaHelperTargetPath("to" "_aos");
  }
  if (canonicalPath == "/to" "_aos_ref") {
    return compatibilitySoaHelperTargetPath("to" "_aos_ref");
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
  const std::string compatibilityPrefix =
      compatibilitySoaHelperTargetPath("") + "/";
  const std::string publicPrefix = publicSoaHelperTargetPath("") + "/";
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
  return canonicalPath == experimentalSoaHelperPathLocal("soa" "VectorCount") ||
         canonicalPath == experimentalSoaHelperPathLocal("soa" "VectorCountRef");
}

bool isExperimentalSoaBorrowedHelperPath(std::string_view path) {
  std::string canonicalPath(path);
  const size_t specializationSuffix = canonicalPath.find("__");
  if (specializationSuffix != std::string::npos) {
    canonicalPath.erase(specializationSuffix);
  }
  return canonicalPath == experimentalSoaHelperPathLocal("soa" "VectorCountRef") ||
         canonicalPath == experimentalSoaHelperPathLocal("soa" "VectorGetRef") ||
         canonicalPath == experimentalSoaHelperPathLocal("soa" "VectorRefRef");
}

bool isExperimentalSoaGetLikeHelperPath(std::string_view path) {
  std::string canonicalPath(path);
  const size_t specializationSuffix = canonicalPath.find("__");
  if (specializationSuffix != std::string::npos) {
    canonicalPath.erase(specializationSuffix);
  }
  return canonicalPath == experimentalSoaHelperPathLocal("soa" "VectorGet") ||
         canonicalPath == experimentalSoaHelperPathLocal("soa" "VectorGetRef");
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
    return helperName == "push" || helperName == "reserve" ||
           helperName == "soa" "VectorPush" ||
           helperName == "soa" "VectorReserve";
  }
  if (canonicalPath.starts_with(experimentalSoaPrefix)) {
    const std::string_view helperName =
        std::string_view(canonicalPath).substr(experimentalSoaPrefix.size());
    return helperName == "push" || helperName == "reserve" ||
           helperName == "soa" "VectorPush" ||
           helperName == "soa" "VectorReserve";
  }
  return false;
}

bool isExperimentalSoaFieldViewHelperPath(std::string_view path) {
  const std::string experimentalSoaFieldViewPrefix =
      experimentalSoaHelperPathLocal("soa" "VectorFieldView");
  const std::string canonicalSoaFieldViewPrefix =
      compatibilitySoaHelperTargetPath("soa" "VectorFieldView");
  const std::string publicSoaFieldViewPath =
      publicSoaHelperTargetPath("field_view");
  constexpr std::string_view kExperimentalSoaColumnFieldViewUnsafePrefix =
      "/std/collections/internal_soa_storage/soaColumnFieldViewUnsafe";
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
             "/std/collections/internal_soa_storage/soaFieldViewRead",
             0) == 0;
}

bool isExperimentalSoaFieldViewRefHelperPath(std::string_view path) {
  std::string canonicalPath(path);
  const size_t specializationSuffix = canonicalPath.find("__");
  if (specializationSuffix != std::string::npos) {
    canonicalPath.erase(specializationSuffix);
  }
  return canonicalPath == "/std/collections/internal_soa_storage/soaFieldViewRef";
}

bool isExperimentalSoaColumnSlotHelperPath(std::string_view path) {
  std::string canonicalPath(path);
  const size_t specializationSuffix = canonicalPath.find("__");
  if (specializationSuffix != std::string::npos) {
    canonicalPath.erase(specializationSuffix);
  }
  return canonicalPath.rfind(
             "/std/collections/internal_soa_storage/soaColumnSlotUnsafe",
             0) == 0;
}

bool isExperimentalSoaColumnFieldSchemaHelperPath(std::string_view path) {
  std::string canonicalPath(path);
  const size_t specializationSuffix = canonicalPath.find("__");
  if (specializationSuffix != std::string::npos) {
    canonicalPath.erase(specializationSuffix);
  }
  return canonicalPath.rfind(
             "/std/collections/internal_soa_storage/soaColumnField",
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
  return canonicalPath == experimentalSoaHelperPathLocal("soa" "VectorRef") ||
         canonicalPath == experimentalSoaHelperPathLocal("soa" "VectorRefRef") ||
         canonicalPath == "/std/collections/internal_soa_storage/soaColumnRef";
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
  return helperName == "soa" "VectorToAos" ||
         helperName == "soa" "VectorToAosRef";
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
      isLegacyOrCanonicalSoaHelperPath(canonicalSoaGetPath, "get_ref")) {
    return canonicalSoaGetPath;
  }
  if (normalizedResolvedPath == samePathSoaHelperTargetPath("count")) {
    return compatibilitySoaHelperTargetPath("count");
  }
  if (normalizedResolvedPath == samePathSoaHelperTargetPath("count_ref")) {
    return compatibilitySoaHelperTargetPath("count_ref");
  }
  return std::string(resolvedPath);
}

} // namespace

std::string soaUnavailableMethodDiagnostic(std::string_view resolvedPath) {
  return "unknown method: " + canonicalSoaPendingHelperPath(resolvedPath);
}

bool getBuiltinArrayAccessName(const Expr &expr, std::string &out) {
  if (expr.name.empty()) {
    return false;
  }
  const std::string rawExprName = expr.name;
  auto stripTemplateSpecializationSuffix = [](std::string value) {
    const size_t suffix = value.find("__t");
    if (suffix != std::string::npos) {
      value.erase(suffix);
    }
    return value;
  };
  auto stripGeneratedSuffix = [](std::string value) {
    const size_t suffix = value.find("__");
    if (suffix != std::string::npos) {
      value.erase(suffix);
    }
    return value;
  };
  auto accessAliasFromMemberName = [&](std::string memberName) -> bool {
    memberName = stripGeneratedSuffix(stripTemplateSpecializationSuffix(std::move(memberName)));
    if (memberName == "at" || memberName == "at_ref" || memberName == "At" ||
        memberName == collectionAliasLocal("vector", "At")) {
      out = "at";
      return true;
    }
    if (memberName == "at_unsafe" || memberName == "at_unsafe_ref" ||
        memberName == "AtUnsafe" || memberName == collectionAliasLocal("vector", "AtUnsafe")) {
      out = "at_unsafe";
      return true;
    }
    return false;
  };
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
  auto matchStdlibLegacyAccessAlias = [&](std::string_view prefix) -> bool {
    if (name.rfind(prefix, 0) != 0) {
      return false;
    }
    const std::string memberName = name.substr(prefix.size());
    return memberName.find('/') == std::string::npos &&
           accessAliasFromMemberName(memberName);
  };
  if (matchStdlibLegacyAccessAlias("std/collections/") ||
      matchStdlibLegacyAccessAlias(experimentalCollectionMemberRootLocal("vector")) ||
      matchStdlibLegacyAccessAlias(experimentalCollectionMemberRootLocal("map"))) {
    return true;
  }
  const std::string stdVectorRoot = collectionMemberRootLocal("vector");
  if (name.rfind(stdVectorRoot, 0) == 0) {
    std::string alias = stripTemplateSpecializationSuffix(name.substr(stdVectorRoot.size()));
    if (accessAliasFromMemberName(alias)) {
      return true;
    }
    return false;
  }
  if (name.rfind("array/", 0) == 0) {
    return false;
  }
  std::string keyValueHelperName;
  if (resolveKeyValueHelperMemberNameLocal(name, keyValueHelperName)) {
    if (accessAliasFromMemberName(keyValueHelperName)) {
      return true;
    }
    return false;
  }
  if (name.find('/') != std::string::npos) {
    std::string rawName = rawExprName;
    if (!rawName.empty() && rawName[0] == '/') {
      rawName.erase(0, 1);
    }
    return rawName.find('/') == std::string::npos &&
           accessAliasFromMemberName(rawName);
  }
  return accessAliasFromMemberName(name);
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
