#include "IrLowererHelpers.h"

#include <algorithm>
#include <array>
#include <string_view>

#include "primec/StdlibSurfaceRegistry.h"

namespace primec::ir_lowerer {
namespace {
std::string stdCollectionsRoot() {
  return "std/collections";
}

std::string collectionMemberRoot(std::string_view collectionName) {
  return stdCollectionsRoot() + "/" + std::string(collectionName) + "/";
}

std::string collectionMemberPath(std::string_view collectionName,
                                 std::string_view memberName) {
  return collectionMemberRoot(collectionName) + std::string(memberName);
}

std::string experimentalCollectionMemberRoot(std::string_view collectionName) {
  return stdCollectionsRoot() + "/experimental_" + std::string(collectionName) + "/";
}

std::string collectionWrapperAlias(std::string_view collectionName,
                                   std::string_view suffix) {
  return std::string(collectionName) + std::string(suffix);
}

bool resolvesKeyValueHelperSurfacePath(std::string_view path) {
  const auto *metadata =
      findStdlibSurfaceMetadataByBridgeKey("collections.map_helpers");
  if (metadata == nullptr) {
    return false;
  }
  if (!resolveStdlibSurfaceMemberName(*metadata, path).empty()) {
    return true;
  }
  if (!path.empty() && path.front() != '/') {
    const std::string rootedPath = "/" + std::string(path);
    return !resolveStdlibSurfaceMemberName(*metadata, rootedPath).empty();
  }
  return false;
}

std::string keyValueConstructorAliasToken() {
  const auto *metadata =
      findStdlibSurfaceMetadataByBridgeKey("collections.map_constructors");
  if (metadata == nullptr) {
    return {};
  }
  for (std::string_view alias : metadata->importAliasSpellings) {
    if (!alias.empty() && alias.find('/') == std::string_view::npos) {
      return std::string(alias);
    }
  }
  return {};
}

std::string stripGeneratedSuffix(std::string alias) {
  const size_t suffix = alias.find("__");
  if (suffix != std::string::npos) {
    alias.erase(suffix);
  }
  return alias;
}

std::string resolveScopedExprName(const Expr &expr) {
  if (expr.name.find('/') != std::string::npos || expr.namespacePrefix.empty()) {
    return expr.name;
  }
  if (expr.namespacePrefix == "/") {
    return "/" + expr.name;
  }
  return expr.namespacePrefix + "/" + expr.name;
}

std::string resolveMathExprName(const Expr &expr) {
  return resolveScopedExprName(expr);
}

bool isNamespacedStdlibBuiltinAlias(const std::string &alias) {
  const std::string keyValueAlias = keyValueConstructorAliasToken();
  return alias == "assign" || alias == "if" || alias == "while" ||
         alias == "take" || alias == "borrow" || alias == "init" ||
         alias == "drop" || alias == "increment" ||
         alias == "decrement" || alias == "return" ||
         alias == "then" || alias == "else" || alias == "do" ||
         alias == "block" || alias == "loop" || alias == "for" ||
         alias == "repeat" || alias == "try" || alias == "location" ||
         alias == "dereference" || alias == "count" ||
         alias == "count_ref" || alias == "capacity" ||
         alias == "to" "_aos" || alias == "to" "_aos_ref" ||
         alias == "push" || alias == "pop" || alias == "reserve" ||
         alias == "clear" || alias == "remove_at" ||
         alias == "remove_swap" || alias == "move" ||
         alias == "negate" || alias == "plus" || alias == "minus" ||
         alias == "multiply" || alias == "divide" ||
         alias == "greater_than" || alias == "less_than" ||
         alias == "equal" || alias == "not_equal" ||
         alias == "greater_equal" || alias == "less_equal" ||
         alias == "and" || alias == "or" || alias == "not" ||
         alias == "get" || alias == "get_ref" || alias == "ref" ||
         alias == "ref_ref" || alias == "at" ||
         alias == "at_unsafe" || alias == "array" ||
         alias == "vector" || (!keyValueAlias.empty() && alias == keyValueAlias) ||
         alias == "soa" "_vector" || alias == "convert" ||
         alias == "clamp" || alias == "min" || alias == "max" ||
         alias == "lerp" || alias == "fma" || alias == "hypot" ||
         alias == "copysign" || alias == "radians" ||
         alias == "degrees" || alias == "sin" || alias == "cos" ||
         alias == "tan" || alias == "atan2" || alias == "asin" ||
         alias == "acos" || alias == "atan" || alias == "sinh" ||
         alias == "cosh" || alias == "tanh" || alias == "asinh" ||
         alias == "acosh" || alias == "atanh" || alias == "exp" ||
         alias == "exp2" || alias == "log" || alias == "log2" ||
         alias == "log10" || alias == "abs" || alias == "sign" ||
         alias == "saturate" || alias == "pow" ||
         alias == "is_nan" || alias == "is_inf" ||
         alias == "is_finite" || alias == "floor" ||
         alias == "ceil" || alias == "round" || alias == "trunc" ||
         alias == "fract" || alias == "sqrt" || alias == "cbrt";
}

bool shouldStripBuiltinPrefix(const std::string &prefix,
                              const std::string &alias) {
  if (prefix == "std/image/" || prefix == "std/ui/") {
    return isNamespacedStdlibBuiltinAlias(alias);
  }
  return true;
}

std::string normalizeInternalSoaStorageBuiltinAlias(std::string name) {
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  const char *const builtinPrefixes[] = {
      "std/collections/internal_soa_storage/",
      "std/collections/internal_buffer_checked/",
      "std/collections/internal_buffer_unchecked/",
      "std/collections/experimental" "_soa" "_vector/",
      "std/collections/experimental" "_soa" "_vector_conversions/",
      "std/collections/" "soa" "_vector_conversions/",
      "std/collections/ContainerError/",
      "std/image/",
      "std/ui/",
  };
  for (const char *prefix : builtinPrefixes) {
    const std::string prefixText(prefix);
    if (name.rfind(prefixText, 0) != 0) {
      continue;
    }
    std::string alias = name.substr(prefixText.size());
    const size_t slash = alias.find_last_of('/');
    if (slash != std::string::npos) {
      alias = alias.substr(slash + 1);
    }
    const size_t generatedSuffix = alias.find("__");
    if (generatedSuffix != std::string::npos) {
      alias.erase(generatedSuffix);
    }
    if (!shouldStripBuiltinPrefix(prefixText, alias)) {
      return name;
    }
    return alias;
  }
  const std::string experimentalVectorPrefix =
      experimentalCollectionMemberRoot("vector");
  if (name.rfind(experimentalVectorPrefix, 0) == 0) {
    std::string alias = name.substr(experimentalVectorPrefix.size());
    const size_t slash = alias.find_last_of('/');
    if (slash != std::string::npos) {
      alias = alias.substr(slash + 1);
    }
    alias = stripGeneratedSuffix(std::move(alias));
    return alias;
  }
  return name;
}

bool parseMathName(const std::string &name, std::string &out, bool allowBare) {
  if (name.empty()) {
    return false;
  }
  std::string normalized = name;
  normalized = normalizeInternalSoaStorageBuiltinAlias(normalized);
  const bool hasLeadingSlash = !normalized.empty() && normalized[0] == '/';
  if (hasLeadingSlash) {
    normalized.erase(0, 1);
  }
  if (normalized.rfind("std/math/", 0) == 0) {
    out = normalized.substr(9);
    return true;
  }
  if (normalized.find('/') != std::string::npos) {
    if (!hasLeadingSlash) {
      return false;
    }
    const size_t slash = normalized.find_last_of('/');
    if (slash == std::string::npos || slash + 1 >= normalized.size()) {
      return false;
    }
    normalized = normalized.substr(slash + 1);
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

} // namespace

bool getBuiltinClampName(const Expr &expr, bool allowBare) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  std::string name;
  if (!parseMathName(resolveMathExprName(expr), name, allowBare)) {
    return false;
  }
  return name == "clamp";
}

bool getBuiltinMinMaxName(const Expr &expr, std::string &out, bool allowBare) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  if (!parseMathName(resolveMathExprName(expr), out, allowBare)) {
    return false;
  }
  return out == "min" || out == "max";
}

bool getBuiltinLerpName(const Expr &expr, std::string &out, bool allowBare) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  if (!parseMathName(resolveMathExprName(expr), out, allowBare)) {
    return false;
  }
  return out == "lerp";
}

bool getBuiltinFmaName(const Expr &expr, std::string &out, bool allowBare) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  if (!parseMathName(resolveMathExprName(expr), out, allowBare)) {
    return false;
  }
  return out == "fma";
}

bool getBuiltinHypotName(const Expr &expr, std::string &out, bool allowBare) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  if (!parseMathName(resolveMathExprName(expr), out, allowBare)) {
    return false;
  }
  return out == "hypot";
}

bool getBuiltinCopysignName(const Expr &expr, std::string &out, bool allowBare) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  if (!parseMathName(resolveMathExprName(expr), out, allowBare)) {
    return false;
  }
  return out == "copysign";
}

bool getBuiltinAngleName(const Expr &expr, std::string &out, bool allowBare) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  if (!parseMathName(resolveMathExprName(expr), out, allowBare)) {
    return false;
  }
  return out == "radians" || out == "degrees";
}

bool getBuiltinTrigName(const Expr &expr, std::string &out, bool allowBare) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  if (!parseMathName(resolveMathExprName(expr), out, allowBare)) {
    return false;
  }
  return out == "sin" || out == "cos" || out == "tan";
}

bool getBuiltinTrig2Name(const Expr &expr, std::string &out, bool allowBare) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  if (!parseMathName(resolveMathExprName(expr), out, allowBare)) {
    return false;
  }
  return out == "atan2";
}

bool getBuiltinArcTrigName(const Expr &expr, std::string &out, bool allowBare) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  if (!parseMathName(resolveMathExprName(expr), out, allowBare)) {
    return false;
  }
  return out == "asin" || out == "acos" || out == "atan";
}

bool getBuiltinHyperbolicName(const Expr &expr, std::string &out, bool allowBare) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  if (!parseMathName(resolveMathExprName(expr), out, allowBare)) {
    return false;
  }
  return out == "sinh" || out == "cosh" || out == "tanh";
}

bool getBuiltinArcHyperbolicName(const Expr &expr, std::string &out, bool allowBare) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  if (!parseMathName(resolveMathExprName(expr), out, allowBare)) {
    return false;
  }
  return out == "asinh" || out == "acosh" || out == "atanh";
}

bool getBuiltinExpName(const Expr &expr, std::string &out, bool allowBare) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  if (!parseMathName(resolveMathExprName(expr), out, allowBare)) {
    return false;
  }
  return out == "exp" || out == "exp2";
}

bool getBuiltinLogName(const Expr &expr, std::string &out, bool allowBare) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  if (!parseMathName(resolveMathExprName(expr), out, allowBare)) {
    return false;
  }
  return out == "log" || out == "log2" || out == "log10";
}

bool getBuiltinAbsSignName(const Expr &expr, std::string &out, bool allowBare) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  if (!parseMathName(resolveMathExprName(expr), out, allowBare)) {
    return false;
  }
  return out == "abs" || out == "sign";
}

bool getBuiltinSaturateName(const Expr &expr, std::string &out, bool allowBare) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  if (!parseMathName(resolveMathExprName(expr), out, allowBare)) {
    return false;
  }
  return out == "saturate";
}

bool getBuiltinPowName(const Expr &expr, std::string &out, bool allowBare) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  if (!parseMathName(resolveMathExprName(expr), out, allowBare)) {
    return false;
  }
  return out == "pow";
}

bool getBuiltinMathPredicateName(const Expr &expr, std::string &out, bool allowBare) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  if (!parseMathName(resolveMathExprName(expr), out, allowBare)) {
    return false;
  }
  return out == "is_nan" || out == "is_inf" || out == "is_finite";
}

bool getBuiltinRoundingName(const Expr &expr, std::string &out, bool allowBare) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  if (!parseMathName(resolveMathExprName(expr), out, allowBare)) {
    return false;
  }
  return out == "floor" || out == "ceil" || out == "round" || out == "trunc" || out == "fract";
}

bool getBuiltinRootName(const Expr &expr, std::string &out, bool allowBare) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  if (!parseMathName(resolveMathExprName(expr), out, allowBare)) {
    return false;
  }
  return out == "sqrt" || out == "cbrt";
}

bool getBuiltinConvertName(const Expr &expr) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  std::string name =
      normalizeInternalSoaStorageBuiltinAlias(resolveScopedExprName(expr));
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  if (name.find('/') != std::string::npos) {
    const size_t slash = name.find_last_of('/');
    if (slash == std::string::npos || slash + 1 >= name.size()) {
      return false;
    }
    name = name.substr(slash + 1);
  }
  return name == "convert";
}

bool getBuiltinMemoryName(const Expr &expr, std::string &out) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  std::string normalized = resolveScopedExprName(expr);
  if (!normalized.empty() && normalized[0] == '/') {
    normalized.erase(0, 1);
  }
  if (normalized.rfind("std/intrinsics/memory/", 0) != 0) {
    return false;
  }
  normalized = normalized.substr(22);
  if (normalized.find('/') != std::string::npos) {
    return false;
  }
  if (normalized == "alloc" || normalized == "free" || normalized == "realloc" || normalized == "at" ||
      normalized == "at_unsafe" || normalized == "reinterpret") {
    out = normalized;
    return true;
  }
  return false;
}

bool getBuiltinGpuName(const Expr &expr, std::string &out) {
  if (expr.name.empty()) {
    return false;
  }
  std::string normalized = resolveScopedExprName(expr);
  if (!normalized.empty() && normalized[0] == '/') {
    normalized.erase(0, 1);
  }
  if (normalized.rfind("std/gpu/", 0) != 0) {
    return false;
  }
  normalized = normalized.substr(8);
  if (normalized.find('/') != std::string::npos) {
    return false;
  }
  if (normalized == "global_id_x" || normalized == "global_id_y" || normalized == "global_id_z") {
    out = normalized;
    return true;
  }
  return false;
}

bool getBuiltinArrayAccessName(const Expr &expr, std::string &out) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  auto matchAccessAlias = [&](const std::string &normalizedName,
                              const std::string &prefix,
                              const std::string &receiverBase) {
    if (normalizedName.rfind(prefix, 0) != 0) {
      return false;
    }
    std::string alias = normalizedName.substr(prefix.size());
    const size_t slash = alias.find('/');
    if (slash != std::string::npos) {
      const std::string receiverPath = alias.substr(0, slash);
      if (receiverPath != receiverBase &&
          receiverPath.rfind(receiverBase + "__", 0) != 0) {
        return false;
      }
      alias = alias.substr(slash + 1);
    }
    alias = stripGeneratedSuffix(std::move(alias));
    if (alias == "at" || alias == "at_ref") {
      out = "at";
      return true;
    }
    if (alias == "at_unsafe" || alias == "at_unsafe_ref") {
      out = "at_unsafe";
      return true;
    }
    return false;
  };
  auto matchLegacyAccessAlias = [&](const std::string &normalizedName,
                                    const std::string &prefix) {
    if (normalizedName.rfind(prefix, 0) != 0) {
      return false;
    }
    std::string alias = normalizedName.substr(prefix.size());
    if (alias.find('/') != std::string::npos) {
      return false;
    }
    alias = stripGeneratedSuffix(std::move(alias));
    if (alias == collectionWrapperAlias("vector", "At")) {
      out = "at";
      return true;
    }
    if (alias == collectionWrapperAlias("vector", "AtUnsafe")) {
      out = "at_unsafe";
      return true;
    }
    return false;
  };
  std::string scopedName = resolveScopedExprName(expr);
  if (!scopedName.empty() && scopedName[0] == '/') {
    scopedName.erase(0, 1);
  }
  std::string rawName = expr.name;
  if (!rawName.empty() && rawName[0] == '/') {
    rawName.erase(0, 1);
  }
  const std::string scopedNameWithoutSuffix = stripGeneratedSuffix(scopedName);
  if (scopedNameWithoutSuffix == collectionMemberPath("vector", "at") ||
      scopedNameWithoutSuffix == collectionMemberPath("vector", "at_unsafe")) {
    return false;
  }
  if (matchAccessAlias(scopedName, collectionMemberRoot("vector"), "Vector")) {
    return true;
  }
  if (matchLegacyAccessAlias(scopedName, stdCollectionsRoot() + "/")) {
    return true;
  }
  if (scopedName.rfind(collectionMemberRoot("vector"), 0) == 0) {
    return false;
  }
  if (matchAccessAlias(scopedName, experimentalCollectionMemberRoot("vector"), "Vector")) {
    return true;
  }
  if (matchLegacyAccessAlias(scopedName, experimentalCollectionMemberRoot("vector"))) {
    return true;
  }
  if (scopedName.rfind(experimentalCollectionMemberRoot("vector"), 0) == 0) {
    return false;
  }
  if (matchLegacyAccessAlias(scopedName, "std/collections/internal_vector/")) {
    return true;
  }
  if (scopedName.rfind("std/collections/internal_vector/", 0) == 0) {
    return false;
  }
  if (matchAccessAlias(scopedName, "std/collections/" "soa" "_vector/", "Soa" "Vector")) {
    return true;
  }
  if (scopedName.rfind("std/collections/" "soa" "_vector/", 0) == 0) {
    std::string alias = scopedName.substr(std::string("std/collections/" "soa" "_vector/").size());
    alias = stripGeneratedSuffix(std::move(alias));
    if (alias == "get" || alias == "get_ref") {
      out = alias;
      return true;
    }
    return false;
  }
  if (scopedName.rfind("soa" "_vector/", 0) == 0) {
    std::string alias = scopedName.substr(std::string("soa" "_vector/").size());
    alias = stripGeneratedSuffix(std::move(alias));
    if (alias == "get" || alias == "get_ref") {
      out = alias;
      return true;
    }
    return false;
  }
  if (matchAccessAlias(scopedName, "std/collections/internal_soa_storage/", "SoaColumn")) {
    return true;
  }
  if (scopedName.rfind("std/collections/internal_soa_storage/", 0) == 0) {
    std::string alias = normalizeInternalSoaStorageBuiltinAlias(scopedName);
    if (alias == "at" || alias == "at_unsafe") {
      out = alias;
      return true;
    }
    return false;
  }
  if (scopedName.rfind("array/", 0) == 0) {
    return false;
  }
  const std::string builtinVectorPrefix = std::string("vector") + "/";
  if (scopedName.rfind(builtinVectorPrefix, 0) == 0) {
    return false;
  }
  if (resolvesKeyValueHelperSurfacePath(scopedName)) {
    return false;
  }
  rawName = stripGeneratedSuffix(std::move(rawName));
  if (rawName.find('/') != std::string::npos) {
    return false;
  }
  if (rawName == "at" || rawName == "at_unsafe") {
    out = rawName;
    return true;
  }
  return false;
}

bool getBuiltinPointerName(const Expr &expr, std::string &out) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  std::string scopedName = resolveScopedExprName(expr);
  if (!scopedName.empty() && scopedName[0] == '/') {
    scopedName.erase(0, 1);
  }
  std::string rawName = expr.name;
  if (!rawName.empty() && rawName[0] == '/') {
    rawName.erase(0, 1);
  }
  if (scopedName == "soa" "_vector/dereference" || scopedName == "soa" "_vector/location") {
    if (scopedName == "soa" "_vector/dereference") {
      out = "dereference";
      return true;
    }
    if (scopedName == "soa" "_vector/location") {
      out = "location";
      return true;
    }
  }
  const std::string helperName =
      normalizeInternalSoaStorageBuiltinAlias(scopedName);
  if (helperName != scopedName) {
    if (helperName == "dereference" || helperName == "location") {
      out = helperName;
      return true;
    }
    return false;
  }
  if (rawName.find('/') != std::string::npos) {
    return false;
  }
  if (rawName == "dereference" || rawName == "location") {
    out = rawName;
    return true;
  }
  return false;
}

bool getBuiltinCollectionName(const Expr &expr, std::string &out) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  std::string scopedName = resolveScopedExprName(expr);
  if (!scopedName.empty() && scopedName[0] == '/') {
    scopedName.erase(scopedName.begin());
  }
  std::string rawName = expr.name;
  if (!rawName.empty() && rawName[0] == '/') {
    rawName.erase(rawName.begin());
  }
  if (scopedName.rfind(collectionMemberRoot("vector"), 0) == 0) {
    std::string alias = scopedName.substr(collectionMemberRoot("vector").size());
    if (alias == "vector") {
      out = "vector";
      return true;
    }
    return false;
  }
  if (scopedName.rfind("std/collections/internal_soa_storage/", 0) == 0) {
    std::string alias = normalizeInternalSoaStorageBuiltinAlias(scopedName);
    if (alias == "array" || alias == "soa" "_vector") {
      out = alias;
      return true;
    }
    return false;
  }
  if (rawName.find('/') != std::string::npos) {
    return false;
  }
  const std::string keyValueAlias = keyValueConstructorAliasToken();
  if (rawName == "array" || rawName == "vector" ||
      (!keyValueAlias.empty() && rawName == keyValueAlias) ||
      rawName == "soa" "_vector") {
    out = rawName;
    return true;
  }
  return false;
}

bool getExperimentalVectorConstructorElementTypeAlias(const Expr &expr,
                                                      std::string &out) {
  out.clear();
  if (expr.kind != Expr::Kind::Call || expr.name.empty() ||
      expr.isMethodCall || !expr.templateArgs.empty()) {
    return false;
  }
  std::string scopedName = resolveScopedExprName(expr);
  return getExperimentalVectorConstructorElementTypeAliasFromPath(scopedName,
                                                                  out);
}

bool getExperimentalVectorConstructorElementTypeAliasFromPath(
    std::string path,
    std::string &out) {
  out.clear();
  if (!path.empty() && path.front() == '/') {
    path.erase(path.begin());
  }
  const std::string prefix = experimentalCollectionMemberRoot("vector");
  if (path.rfind(prefix, 0) != 0) {
    return false;
  }
  std::string alias = path.substr(prefix.size());
  if (alias.empty() || alias.find('/') != std::string::npos ||
      alias.find("__") != std::string::npos) {
    return false;
  }
  const std::array<std::string, 33> reservedVectorAliases = {{
      "Vector",
      "vector",
      collectionWrapperAlias("vector", "New"),
      collectionWrapperAlias("vector", "Single"),
      collectionWrapperAlias("vector", "Pair"),
      collectionWrapperAlias("vector", "Triple"),
      collectionWrapperAlias("vector", "Quad"),
      collectionWrapperAlias("vector", "Quint"),
      collectionWrapperAlias("vector", "Sext"),
      collectionWrapperAlias("vector", "Sept"),
      collectionWrapperAlias("vector", "Oct"),
      collectionWrapperAlias("vector", "Alloc"),
      collectionWrapperAlias("vector", "SlotUnsafe"),
      collectionWrapperAlias("vector", "DataPtr"),
      collectionWrapperAlias("vector", "InitSlot"),
      collectionWrapperAlias("vector", "DropSlot"),
      collectionWrapperAlias("vector", "TakeSlot"),
      collectionWrapperAlias("vector", "BorrowSlot"),
      collectionWrapperAlias("vector", "DropRange"),
      collectionWrapperAlias("vector", "MovePrefixToBuffer"),
      collectionWrapperAlias("vector", "CheckShape"),
      collectionWrapperAlias("vector", "CheckIndex"),
      collectionWrapperAlias("vector", "ReserveInternal"),
      collectionWrapperAlias("vector", "Count"),
      collectionWrapperAlias("vector", "Capacity"),
      collectionWrapperAlias("vector", "Push"),
      collectionWrapperAlias("vector", "Pop"),
      collectionWrapperAlias("vector", "Reserve"),
      collectionWrapperAlias("vector", "Clear"),
      collectionWrapperAlias("vector", "RemoveAt"),
      collectionWrapperAlias("vector", "RemoveSwap"),
      collectionWrapperAlias("vector", "At"),
      collectionWrapperAlias("vector", "AtUnsafe"),
  }};
  if (std::find(reservedVectorAliases.begin(), reservedVectorAliases.end(), alias) !=
      reservedVectorAliases.end()) {
    return false;
  }
  const char *const reservedAliases[] = {
      "and",
      "array",
      "assign",
      "at",
      "count",
      "do",
      "else",
      "equal",
      "greater_than",
      "if",
      "increment",
      "less_than",
      "not",
      "not_equal",
      "or",
      "plus",
      "return",
      "then",
      "while",
  };
  for (const char *reserved : reservedAliases) {
    if (alias == reserved) {
      return false;
    }
  }
  out = std::move(alias);
  return true;
}

} // namespace primec::ir_lowerer
