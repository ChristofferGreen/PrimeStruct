#include "EmitterBuiltinCallPathHelpersInternal.h"
#include "EmitterBuiltinMethodResolutionTypeInferenceInternal.h"
#include "EmitterHelpers.h"
#include "primec/StringLiteral.h"

#include <array>
#include <string_view>

namespace primec::emitter {

namespace {
constexpr std::string_view KeyValueHelperSurfaceBridgeKey = "collections.map_helpers";
constexpr std::string_view KeyValueConstructorSurfaceBridgeKey =
    "collections.map_constructors";

const StdlibSurfaceMetadata *keyValueHelperSurfaceMetadataLocal() {
  return findStdlibSurfaceMetadataByBridgeKey(KeyValueHelperSurfaceBridgeKey);
}

const StdlibSurfaceMetadata *keyValueConstructorSurfaceMetadataLocal() {
  return findStdlibSurfaceMetadataByBridgeKey(KeyValueConstructorSurfaceBridgeKey);
}

std::string keyValueConstructorAliasToken() {
  const auto *metadata = keyValueConstructorSurfaceMetadataLocal();
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

bool isScopedBuiltinControlAlias(const std::string &name) {
  return name == "return" || name == "then" || name == "else" ||
         name == "if" || name == "block" || name == "loop" ||
         name == "while" || name == "for" || name == "repeat" ||
         name == "do";
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
  if (prefix == "std/file/" || prefix == "std/image/" ||
      prefix == "std/ui/") {
    return isNamespacedStdlibBuiltinAlias(alias);
  }
  return true;
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

} // namespace

std::string resolveExprPath(const Expr &expr);

std::string preferredFileMethodTargetLocal(
    const std::string &methodName,
    const std::unordered_map<std::string, const Definition *> &defMap) {
  const std::string stdlibPath = "/File/" + methodName;
  if (defMap.find(stdlibPath) != defMap.end()) {
    return stdlibPath;
  }
  return "/file/" + methodName;
}

bool isMapCollectionTypeNameLocal(const std::string &name) {
  const std::string normalized = normalizeBindingTypeName(name);
  const auto *metadata = keyValueConstructorSurfaceMetadataLocal();
  bool matchesMapImportAlias = false;
  if (metadata != nullptr) {
    for (std::string_view alias : metadata->importAliasSpellings) {
      if (normalizeBindingTypeName(std::string(alias)) == normalized) {
        matchesMapImportAlias = true;
        break;
      }
    }
  }
  const std::string experimentalMapType =
      experimentalCollectionMemberRootLocal(keyValueConstructorAliasToken()) + "Map";
  return matchesMapImportAlias || normalized == "Map" ||
         normalized == experimentalMapType;
}

bool extractMapKeyValueTypesFromTypeTextLocal(const std::string &typeText,
                                              std::string &keyTypeOut,
                                              std::string &valueTypeOut) {
  keyTypeOut.clear();
  valueTypeOut.clear();
  std::string normalizedType = normalizeBindingTypeName(typeText);
  while (true) {
    std::string base;
    std::string argText;
    if (!splitTemplateTypeName(normalizedType, base, argText)) {
      return false;
    }
    base = normalizeBindingTypeName(base);
    if (isMapCollectionTypeNameLocal(base)) {
      std::vector<std::string> parts;
      if (!splitTopLevelTemplateArgs(argText, parts) || parts.size() != 2) {
        return false;
      }
      keyTypeOut = parts[0];
      valueTypeOut = parts[1];
      return true;
    }
    if (base == "Reference" || base == "Pointer") {
      std::vector<std::string> args;
      if (!splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
        return false;
      }
      normalizedType = normalizeBindingTypeName(args.front());
      continue;
    }
    return false;
  }
}

bool extractMapKeyValueTypesLocal(const BindingInfo &binding,
                                  std::string &keyTypeOut,
                                  std::string &valueTypeOut) {
  if (binding.typeTemplateArg.empty()) {
    return extractMapKeyValueTypesFromTypeTextLocal(binding.typeName, keyTypeOut, valueTypeOut);
  }
  return extractMapKeyValueTypesFromTypeTextLocal(binding.typeName + "<" + binding.typeTemplateArg + ">",
                                                  keyTypeOut,
                                                  valueTypeOut);
}

bool allowsArrayVectorCompatibilitySuffix(const std::string &suffix) {
  std::string canonicalMemberName;
  return !resolvePublishedCollectionSurfaceMemberToken(
      suffix,
      StdlibSurfaceId::CollectionsVectorHelperSurface,
      canonicalMemberName);
}

std::string normalizedExprPath(const Expr &expr) {
  std::string path = resolveExprPath(expr);
  if (!path.empty() && path.front() != '/') {
    path.insert(path.begin(), '/');
  }
  return path;
}

bool isCanonicalVectorHelperExprPath(const Expr &expr) {
  const auto *metadata =
      findStdlibSurfaceMetadataByBridgeKey("collections.vector_helpers");
  if (metadata == nullptr) {
    return false;
  }
  const std::string prefix = std::string(metadata->canonicalPath) + "/";
  return normalizedExprPath(expr).rfind(prefix, 0) == 0;
}

bool isCanonicalStdlibSurfaceExprPath(const Expr &expr, StdlibSurfaceId surfaceId) {
  const auto *metadata = findStdlibSurfaceMetadata(surfaceId);
  if (metadata == nullptr) {
    return false;
  }
  const std::string path = normalizedExprPath(expr);
  if (metadata->shape == StdlibSurfaceShape::ConstructorFamily) {
    return path == metadata->canonicalPath ||
           path.rfind(std::string(metadata->canonicalPath) + "__", 0) == 0;
  }
  const std::string prefix = std::string(metadata->canonicalPath) + "/";
  return path.rfind(prefix, 0) == 0;
}

bool isCanonicalStdlibSurfaceExprPath(const Expr &expr,
                                      const StdlibSurfaceMetadata &metadata) {
  const std::string path = normalizedExprPath(expr);
  if (metadata.shape == StdlibSurfaceShape::ConstructorFamily) {
    return path == metadata.canonicalPath ||
           path.rfind(std::string(metadata.canonicalPath) + "__", 0) == 0;
  }
  const std::string prefix = std::string(metadata.canonicalPath) + "/";
  return path.rfind(prefix, 0) == 0;
}

bool resolveCanonicalStdlibSurfaceExprMemberName(const Expr &expr,
                                                 StdlibSurfaceId surfaceId,
                                                 std::string &memberNameOut) {
  if (!isCanonicalStdlibSurfaceExprPath(expr, surfaceId)) {
    return false;
  }
  return resolvePublishedCollectionSurfaceExprMemberName(
      expr, surfaceId, memberNameOut);
}

bool resolveStdlibSurfaceExprMemberNameLocal(const Expr &expr,
                                             const StdlibSurfaceMetadata &metadata,
                                             bool canonicalOnly,
                                             std::string &memberNameOut) {
  memberNameOut.clear();
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  if (canonicalOnly && !isCanonicalStdlibSurfaceExprPath(expr, metadata)) {
    return false;
  }
  const std::string normalizedPath = normalizedExprPath(expr);
  if (const std::string_view memberName =
          resolveStdlibSurfaceMemberName(metadata, normalizedPath);
      !memberName.empty()) {
    memberNameOut.assign(memberName);
    return true;
  }
  if (metadata.bridgeKey == KeyValueHelperSurfaceBridgeKey &&
      normalizedPath.rfind("/std/collections/Map", 0) == 0) {
    const std::string memberToken =
        normalizedPath.substr(std::string("/std/collections/Map").size());
    if (const std::string_view memberName =
            resolveStdlibSurfaceMemberName(metadata, memberToken);
        !memberName.empty()) {
      memberNameOut.assign(memberName);
      return true;
    }
  }
  return false;
}

bool resolveCanonicalMapHelperExprMemberName(const Expr &expr,
                                             std::string &memberNameOut) {
  const auto *metadata = keyValueHelperSurfaceMetadataLocal();
  return metadata != nullptr &&
         resolveStdlibSurfaceExprMemberNameLocal(
             expr, *metadata, true, memberNameOut);
}

bool resolvePublishedMapHelperExprMemberName(const Expr &expr,
                                             std::string &memberNameOut) {
  const auto *metadata = keyValueHelperSurfaceMetadataLocal();
  return metadata != nullptr &&
         resolveStdlibSurfaceExprMemberNameLocal(
             expr, *metadata, false, memberNameOut);
}

bool resolveCanonicalMapConstructorExprMemberName(const Expr &expr,
                                                  std::string &memberNameOut) {
  const auto *metadata = keyValueConstructorSurfaceMetadataLocal();
  return metadata != nullptr &&
         resolveStdlibSurfaceExprMemberNameLocal(
             expr, *metadata, true, memberNameOut);
}

bool resolvePublishedMapConstructorExprMemberName(const Expr &expr,
                                                  std::string &memberNameOut) {
  const auto *metadata = keyValueConstructorSurfaceMetadataLocal();
  return metadata != nullptr &&
         resolveStdlibSurfaceExprMemberNameLocal(
             expr, *metadata, false, memberNameOut);
}

bool resolvePublishedVectorHelperExprMemberName(const Expr &expr,
                                                std::string &memberNameOut) {
  if (!isCanonicalVectorHelperExprPath(expr)) {
    return false;
  }
  return resolvePublishedCollectionSurfaceExprMemberName(
      expr, StdlibSurfaceId::CollectionsVectorHelperSurface, memberNameOut);
}

bool resolvePublishedVectorConstructorExprMemberName(
    const Expr &expr,
    std::string &memberNameOut) {
  const auto *metadata =
      findStdlibSurfaceMetadataByBridgeKey("collections.vector_constructors");
  if (metadata == nullptr) {
    return false;
  }
  const std::string path = normalizedExprPath(expr);
  if (path != metadata->canonicalPath &&
      path.rfind(std::string(metadata->canonicalPath) + "__", 0) != 0) {
    return false;
  }
  return resolvePublishedCollectionSurfaceExprMemberName(
      expr, StdlibSurfaceId::CollectionsVectorConstructors, memberNameOut);
}

std::string canonicalVectorHelperPathForSuffix(const std::string &suffix) {
  const auto *metadata =
      findStdlibSurfaceMetadataByBridgeKey("collections.vector_helpers");
  if (metadata == nullptr) {
    return "";
  }
  return std::string(metadata->canonicalPath) + "/" + suffix;
}

std::string normalizeInternalSoaStorageBuiltinAlias(const std::string &path) {
  std::string name = path;
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  const std::array<std::string, 11> builtinPrefixes = {{
      "std/collections/internal_soa_storage/",
      "std/collections/internal_buffer_checked/",
      "std/collections/internal_buffer_unchecked/",
      "std/collections/experimental" "_soa" "_vector/",
      "std/collections/experimental" "_soa" "_vector_conversions/",
      "std/collections/" "soa" "_vector_conversions/",
      experimentalCollectionMemberRootLocal("vector"),
      "std/collections/ContainerError/",
      "std/file/",
      "std/image/",
      "std/ui/",
  }};
  for (const std::string &prefixText : builtinPrefixes) {
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
  std::string alias = name;
  const size_t slash = alias.find_last_of('/');
  if (slash != std::string::npos) {
    alias = alias.substr(slash + 1);
  }
  if (isScopedBuiltinControlAlias(alias)) {
    return alias;
  }
  return name;
}

bool getBuiltinArrayAccessNameLocal(const Expr &expr, std::string &out) {
  if (expr.name.empty()) {
    return false;
  }
  auto stripGeneratedSuffix = [](std::string alias) {
    const size_t generatedSuffix = alias.find("__");
    if (generatedSuffix != std::string::npos) {
      alias.erase(generatedSuffix);
    }
    return alias;
  };
  auto matchAccessAlias = [&](const std::string &normalizedName,
                              const char *prefix,
                              const char *receiverBase) {
    const std::string prefixText(prefix);
    if (normalizedName.rfind(prefixText, 0) != 0) {
      return false;
    }
    std::string alias = normalizedName.substr(prefixText.size());
    const size_t slash = alias.find('/');
    if (slash != std::string::npos) {
      const std::string receiverPath = alias.substr(0, slash);
      const std::string receiverBaseText(receiverBase);
      if (receiverPath != receiverBaseText &&
          receiverPath.rfind(receiverBaseText + "__", 0) != 0) {
        return false;
      }
      alias = alias.substr(slash + 1);
    }
    alias = stripGeneratedSuffix(std::move(alias));
    if (alias == "at" || alias == "at_unsafe") {
      out = alias;
      return true;
    }
    return false;
  };
  auto matchLegacyAccessAlias = [&](const std::string &normalizedName,
                                    std::string_view prefixText) {
    if (normalizedName.rfind(prefixText, 0) != 0) {
      return false;
    }
    std::string alias = normalizedName.substr(prefixText.size());
    if (alias.find('/') != std::string::npos) {
      return false;
    }
    alias = stripGeneratedSuffix(std::move(alias));
    if (alias == collectionAliasLocal("vector", "At")) {
      out = "at";
      return true;
    }
    if (alias == collectionAliasLocal("vector", "AtUnsafe")) {
      out = "at_unsafe";
      return true;
    }
    return false;
  };
  const std::string resolvedPath = resolveExprPath(expr);
  if (resolvePublishedVectorHelperExprMemberName(expr, out)) {
    return out == "at" || out == "at_unsafe";
  }
  if (resolveCanonicalMapHelperExprMemberName(expr, out)) {
    return isCanonicalMapAccessHelperName(out);
  }
  std::string scopedName = resolvedPath;
  if (!scopedName.empty() && scopedName[0] == '/') {
    scopedName.erase(0, 1);
  }
  if (matchAccessAlias(scopedName,
                       "std/collections/internal_soa_storage/",
                       "SoaColumn")) {
    return true;
  }
  if (matchLegacyAccessAlias(scopedName, "std/collections/") ||
      matchLegacyAccessAlias(scopedName,
                             experimentalCollectionMemberRootLocal("vector"))) {
    return true;
  }
  if (scopedName.rfind("std/collections/internal_soa_storage/", 0) == 0) {
    std::string alias = stripGeneratedSuffix(
        scopedName.substr(
            std::string("std/collections/internal_soa_storage/").size()));
    if (alias == "at" || alias == "at_unsafe") {
      out = alias;
      return true;
    }
    return false;
  }
  std::string name = expr.name;
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  if (name.find('/') != std::string::npos) {
    out.clear();
    return false;
  }
  if (isCanonicalMapAccessHelperName(name)) {
    out = name;
    return true;
  }
  out.clear();
  return false;
}

bool getBuiltinOperator(const Expr &expr, char &out) {
  if (expr.name.empty()) {
    return false;
  }
  std::string name = normalizeInternalSoaStorageBuiltinAlias(resolveExprPath(expr));
  if (name.rfind("std/gpu/", 0) == 0) {
    name.erase(0, 8);
  }
  if (name.find('/') != std::string::npos) {
    return false;
  }
  if (name == "plus") {
    out = '+';
    return true;
  }
  if (name == "minus") {
    out = '-';
    return true;
  }
  if (name == "multiply") {
    out = '*';
    return true;
  }
  if (name == "divide") {
    out = '/';
    return true;
  }
  return false;
}

bool getBuiltinComparison(const Expr &expr, const char *&out) {
  if (expr.name.empty()) {
    return false;
  }
  std::string name = normalizeInternalSoaStorageBuiltinAlias(resolveExprPath(expr));
  if (name.find('/') != std::string::npos) {
    return false;
  }
  if (name == "greater_than") {
    out = ">";
    return true;
  }
  if (name == "less_than") {
    out = "<";
    return true;
  }
  if (name == "equal") {
    out = "==";
    return true;
  }
  if (name == "not_equal") {
    out = "!=";
    return true;
  }
  if (name == "greater_equal") {
    out = ">=";
    return true;
  }
  if (name == "less_equal") {
    out = "<=";
    return true;
  }
  if (name == "and") {
    out = "&&";
    return true;
  }
  if (name == "or") {
    out = "||";
    return true;
  }
  if (name == "not") {
    out = "!";
    return true;
  }
  return false;
}

bool getBuiltinMutationName(const Expr &expr, std::string &out) {
  if (expr.name.empty()) {
    return false;
  }
  std::string name = normalizeInternalSoaStorageBuiltinAlias(resolveExprPath(expr));
  if (name.find('/') != std::string::npos) {
    return false;
  }
  if (name == "increment" || name == "decrement") {
    out = name;
    return true;
  }
  return false;
}

bool isSimpleCallName(const Expr &expr, const char *nameToMatch) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  const std::string targetName = nameToMatch == nullptr ? std::string() : std::string(nameToMatch);
  auto isInternalSoaStorageBareBuiltin = [](const std::string &name) {
    return name == "assign" || name == "if" || name == "while" || name == "take" ||
           name == "borrow" || name == "init" || name == "drop" || name == "increment" ||
           name == "decrement" || name == "return" || name == "then" || name == "else" ||
           name == "do" || name == "block" || name == "loop" || name == "for" ||
           name == "repeat" || name == "try" || name == "location" || name == "dereference" ||
           name == "count" || name == "count_ref" ||
           name == "capacity" || name == "to" "_aos" ||
           name == "to" "_aos_ref" ||
           name == "push" || name == "reserve" ||
           name == "move" || name == "negate" ||
           name == "plus" || name == "minus" || name == "multiply" ||
           name == "divide" || name == "greater_than" ||
           name == "less_than" || name == "equal" || name == "not_equal" ||
           name == "greater_equal" || name == "less_equal" ||
           name == "and" || name == "or" || name == "not" ||
           name == "get" || name == "get_ref" ||
           name == "ref" || name == "ref_ref";
  };
  auto matchScopedBuiltinTail = [&](const std::string &candidate) {
    std::string alias = candidate;
    const size_t slash = alias.find_last_of('/');
    if (slash != std::string::npos) {
      alias = alias.substr(slash + 1);
    }
    if (alias.find('/') == std::string::npos &&
        isInternalSoaStorageBareBuiltin(alias)) {
      return alias == targetName;
    }
    return false;
  };
  const std::string resolvedPath = resolveExprPath(expr);
  std::string helperName;
  if (resolvePublishedVectorHelperExprMemberName(expr, helperName)) {
    return helperName == targetName;
  }
  if (resolveCanonicalMapHelperExprMemberName(expr, helperName)) {
    return (helperName == "count" || helperName == "at" ||
            helperName == "at_unsafe") &&
           helperName == targetName;
  }
  if (resolveCanonicalMapConstructorExprMemberName(expr, helperName)) {
    return helperName == targetName;
  }
  const std::string internalSoaAlias =
      normalizeInternalSoaStorageBuiltinAlias(resolvedPath);
  if (internalSoaAlias.find('/') == std::string::npos &&
      isInternalSoaStorageBareBuiltin(internalSoaAlias)) {
    return internalSoaAlias == targetName;
  }
  if (resolvedPath.find('/') != std::string::npos &&
      matchScopedBuiltinTail(resolvedPath)) {
    return true;
  }
  std::string name = expr.name;
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  if (name.find('/') != std::string::npos) {
    return false;
  }
  return name == targetName;
}

bool getPrintBuiltin(const Expr &expr, PrintBuiltin &out) {
  if (isSimpleCallName(expr, "print")) {
    out.target = PrintTarget::Out;
    out.newline = false;
    out.name = "print";
    return true;
  }
  if (isSimpleCallName(expr, "print_line")) {
    out.target = PrintTarget::Out;
    out.newline = true;
    out.name = "print_line";
    return true;
  }
  if (isSimpleCallName(expr, "print_error")) {
    out.target = PrintTarget::Err;
    out.newline = false;
    out.name = "print_error";
    return true;
  }
  if (isSimpleCallName(expr, "print_line_error")) {
    out.target = PrintTarget::Err;
    out.newline = true;
    out.name = "print_line_error";
    return true;
  }
  return false;
}

bool isPathSpaceBuiltinName(const Expr &expr) {
  return isSimpleCallName(expr, "notify") || isSimpleCallName(expr, "insert") || isSimpleCallName(expr, "take");
}

std::string stripStringLiteralSuffix(const std::string &token) {
  std::string literalText;
  std::string suffix;
  if (!splitStringLiteralToken(token, literalText, suffix)) {
    return token;
  }
  bool raw = false;
  if (suffix == "raw_utf8" || suffix == "raw_ascii") {
    raw = true;
  } else if (suffix.empty() || suffix == "utf8" || suffix == "ascii") {
    raw = false;
  } else {
    return token;
  }

  std::string decoded;
  std::string error;
  if (!decodeStringLiteralText(literalText, decoded, error, raw)) {
    return literalText;
  }

  std::string escaped;
  escaped.reserve(decoded.size() + 2);
  escaped.push_back('"');
  for (char c : decoded) {
    switch (c) {
      case '\\':
        escaped += "\\\\";
        break;
      case '"':
        escaped += "\\\"";
        break;
      case '\n':
        escaped += "\\n";
        break;
      case '\r':
        escaped += "\\r";
        break;
      case '\t':
        escaped += "\\t";
        break;
      case '\0':
        escaped += "\\0";
        break;
      default:
        escaped.push_back(c);
        break;
    }
  }
  escaped.push_back('"');
  return escaped;
}

bool isBuiltinNegate(const Expr &expr) {
  if (expr.name.empty()) {
    return false;
  }
  std::string name = resolveExprPath(expr);
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  if (name.find('/') != std::string::npos) {
    return false;
  }
  return name == "negate";
}

namespace {

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

} // namespace

bool isBuiltinClamp(const Expr &expr, bool allowBare) {
  std::string name;
  if (!parseMathName(resolveExprPath(expr), name, allowBare)) {
    return false;
  }
  return name == "clamp";
}

bool getBuiltinMinMaxName(const Expr &expr, std::string &out, bool allowBare) {
  if (!parseMathName(resolveExprPath(expr), out, allowBare)) {
    return false;
  }
  return out == "min" || out == "max";
}

bool getBuiltinAbsSignName(const Expr &expr, std::string &out, bool allowBare) {
  if (!parseMathName(resolveExprPath(expr), out, allowBare)) {
    return false;
  }
  return out == "abs" || out == "sign";
}

bool getBuiltinSaturateName(const Expr &expr, std::string &out, bool allowBare) {
  if (!parseMathName(resolveExprPath(expr), out, allowBare)) {
    return false;
  }
  return out == "saturate";
}

bool getBuiltinMathName(const Expr &expr, std::string &out, bool allowBare) {
  if (!parseMathName(resolveExprPath(expr), out, allowBare)) {
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

bool isBuiltinMathConstantName(const std::string &name, bool allowBare) {
  std::string candidate;
  if (!parseMathName(name, candidate, allowBare)) {
    return false;
  }
  return candidate == "pi" || candidate == "tau" || candidate == "e";
}

bool getBuiltinPointerOperator(const Expr &expr, char &out) {
  if (expr.name.empty()) {
    return false;
  }
  std::string name = normalizeInternalSoaStorageBuiltinAlias(resolveExprPath(expr));
  if (name == "dereference") {
    out = '*';
    return true;
  }
  if (name == "location") {
    out = '&';
    return true;
  }
  if (name.find('/') != std::string::npos) {
    return false;
  }
  return false;
}

bool getBuiltinMemoryName(const Expr &expr, std::string &out) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  std::string name = resolveExprPath(expr);
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  if (name.rfind("std/intrinsics/memory/", 0) != 0) {
    return false;
  }
  name = name.substr(std::string("std/intrinsics/memory/").size());
  if (name.find('/') != std::string::npos) {
    return false;
  }
  if (name == "alloc" || name == "free" || name == "realloc" ||
      name == "at" || name == "at_unsafe" || name == "reinterpret") {
    out = name;
    return true;
  }
  return false;
}

bool getBuiltinConvertName(const Expr &expr, std::string &out) {
  if (expr.name.empty()) {
    return false;
  }
  std::string name =
      normalizeInternalSoaStorageBuiltinAlias(resolveExprPath(expr));
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
  auto isMapEntryCallExpr = [](const Expr &candidate) {
    if (candidate.kind != Expr::Kind::Call || candidate.name.empty()) {
      return false;
    }
    std::string helperName;
    return resolveCanonicalMapHelperExprMemberName(candidate, helperName) &&
           helperName == "entry";
  };
  const bool hasEntryCtorArgs = [&]() {
    for (const auto &arg : expr.args) {
      if (isMapEntryCallExpr(arg)) {
        return true;
      }
    }
    return false;
  }();
  std::string helperName;
  const std::string resolvedPath = resolveExprPath(expr);
  if (resolvePublishedMapConstructorExprMemberName(expr, helperName) &&
      helperName == "map") {
    if (hasEntryCtorArgs) {
      return false;
    }
    out = "map";
    return true;
  }
  std::string scopedName = resolvedPath;
  if (!scopedName.empty() && scopedName[0] == '/') {
    scopedName.erase(0, 1);
  }
  std::string rawName = expr.name;
  if (!rawName.empty() && rawName[0] == '/') {
    rawName.erase(0, 1);
  }
  if (resolvePublishedVectorHelperExprMemberName(expr, helperName) ||
      resolvePublishedVectorConstructorExprMemberName(expr, helperName)) {
    return false;
  }
  if (resolveCanonicalMapHelperExprMemberName(expr, helperName)) {
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
  if (resolvePublishedMapHelperExprMemberName(expr, helperName)) {
    return false;
  }
  if (rawName.find('/') != std::string::npos) {
    return false;
  }
  const std::string keyValueAlias = keyValueConstructorAliasToken();
  if (rawName == "array" || rawName == "vector" ||
      (!keyValueAlias.empty() && rawName == keyValueAlias) ||
      rawName == "soa" "_vector" || rawName == "soa") {
    out = rawName == "soa" ? "soa" "_vector" : rawName;
    return true;
  }
  return false;
}

std::string resolveExprPath(const Expr &expr) {
  if (!expr.name.empty() && expr.name[0] == '/') {
    return expr.name;
  }
  if (!expr.namespacePrefix.empty()) {
    return expr.namespacePrefix + "/" + expr.name;
  }
  return "/" + expr.name;
}

std::string preferVectorStdlibHelperPath(const std::string &path,
                                         const std::unordered_map<std::string, std::string> &nameMap) {
  std::string preferred = path;
  if (preferred.rfind("/array/", 0) == 0 && nameMap.count(preferred) == 0) {
    const std::string suffix = preferred.substr(std::string("/array/").size());
    if (allowsArrayVectorCompatibilitySuffix(suffix)) {
      const std::string stdlibAlias = canonicalVectorHelperPathForSuffix(suffix);
      if (nameMap.count(stdlibAlias) > 0) {
        return stdlibAlias;
      }
    }
  }
  return preferred;
}

} // namespace primec::emitter
