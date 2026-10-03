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

std::string samePathSoaHelperTargetPath(std::string_view helperName) {
  if (collection_helpers::isToAosHelperName(helperName)) {
    return "/" + std::string(helperName);
  }
  return "/" + soa_paths::publicSoaFolder() + "/" + std::string(helperName);
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
  return collection_helpers::isCountHelperName(helperName) ||
         collection_helpers::isGetHelperName(helperName) ||
         collection_helpers::isRefHelperName(helperName);
}

bool isExplicitPublicSoaSurfaceHelperName(std::string_view helperName) {
  return isSoaReadRefHelperName(helperName) ||
         helperName == "soa" || helperName == "single" ||
         helperName == "from_aos" || helperName == "field_view";
}

bool isSupportedCompatibilitySoaHelperName(std::string_view helperName) {
  return isSoaReadRefHelperName(helperName) ||
         collection_helpers::isToAosHelperName(helperName) ||
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
    return collection_helpers::isToAosHelperName(helperName) ||
           (!usesPublicSurface && helperName == "to_soa");
  }
  if (isPublicSoaSurfaceNamespace(normalizedPrefix) &&
      normalizedName == "to_aos") {
    return true;
  }
  if (isCompatibilitySoaSurfaceNamespace(normalizedPrefix) &&
      (normalizedName == "to_soa" || collection_helpers::isToAosHelperName(normalizedName))) {
    return true;
  }
  return normalizedName == "to_soa" || collection_helpers::isToAosHelperName(normalizedName);
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
  // Pure function of `leadingSlash` alone (both inputs it builds from are
  // static/immutable for the process lifetime) - called deep inside
  // per-expression-node monomorphization recursion, so memoize both
  // outcomes instead of rebuilding the string via concatenation on every
  // call.
  // TODO-5235: built via systemHeapValue() so these magic statics' backing
  // memory is never arena-allocated - see docs/CompilerArenaAllocator.md.
  static const std::string unrooted = primec::systemHeapValue([] {
    std::string path = experimentalCollectionMemberRootLocal(
        internalSoaCollectionTypeName());
    path += experimentalSoaStorageTypeName();
    return path;
  });
  static const std::string rooted = primec::systemHeapValue([] { return "/" + unrooted; });
  return leadingSlash ? rooted : unrooted;
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
         normalized == "to_soa" || collection_helpers::isToAosHelperName(normalized) ||
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
      receiverPath == compatibilitySoaHelperTargetPath("") ||
      receiverPath == "/" + soa_paths::publicSoaFolder() ||
      receiverPath == publicSoaHelperTargetPath("");
  if (isSoaVectorReceiver) {
    const std::string aliasPrefix = soa_paths::legacySoaFolder() + "/";
    const std::string stdPrefix =
        collectionPathPrefixLocal(soa_paths::legacySoaFolder());
    const std::string publicAliasPrefix = soa_paths::publicSoaFolder() + "/";
    const std::string publicStdPrefix =
        collectionPathPrefixLocal(soa_paths::publicSoaFolder());
    if (rawMethodName.rfind(aliasPrefix, 0) == 0) {
      helperName = std::string_view(rawMethodName).substr(aliasPrefix.size());
    } else if (rawMethodName.rfind(stdPrefix, 0) == 0) {
      helperName =
          std::string_view(rawMethodName).substr(stdPrefix.size());
    } else if (rawMethodName.rfind(publicAliasPrefix, 0) == 0) {
      helperName =
          std::string_view(rawMethodName).substr(publicAliasPrefix.size());
    } else if (rawMethodName.rfind(publicStdPrefix, 0) == 0) {
      helperName =
          std::string_view(rawMethodName).substr(publicStdPrefix.size());
    }
    return !helperName.empty() && isRemovedBorrowedSoaCompatibilityHelper(helperName);
  }

  const bool isVectorFamilyReceiver = collection_helpers::isCollectionFamilyRoot(receiverPath, collection_helpers::CollectionFamily::Array) || collection_helpers::isCollectionFamilyRoot(receiverPath, collection_helpers::CollectionFamily::Vector);
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

  if (!collection_helpers::isCollectionFamilyRoot(receiverPath, collection_helpers::CollectionFamily::Map)) {
    return false;
  }
  std::string resolvedKeyValueHelperName;
  if (resolveKeyValueHelperMemberNameLocal(rawMethodName,
                                           resolvedKeyValueHelperName)) {
    helperName = resolvedKeyValueHelperName;
  }
  return !helperName.empty() && isRemovedKeyValueCompatibilityHelper(helperName);
}

} // namespace primec::semantics
