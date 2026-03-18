#include "IrLowererSetupTypeHelpers.h"

#include <algorithm>
#include <functional>

#include "IrLowererCallHelpers.h"
#include "IrLowererHelpers.h"
#include "IrLowererStructTypeHelpers.h"
#include "IrLowererTemplateTypeParseHelpers.h"

namespace primec::ir_lowerer {

namespace {

bool allowsArrayVectorCompatibilitySuffix(const std::string &suffix) {
  return suffix != "count" && suffix != "capacity" && suffix != "at" && suffix != "at_unsafe" &&
         suffix != "push" && suffix != "pop" && suffix != "reserve" && suffix != "clear" &&
         suffix != "remove_at" && suffix != "remove_swap";
}

bool allowsVectorStdlibCompatibilitySuffix(const std::string &suffix) {
  return suffix != "count" && suffix != "capacity" && suffix != "at" && suffix != "at_unsafe" &&
         suffix != "push" && suffix != "pop" && suffix != "reserve" && suffix != "clear" &&
         suffix != "remove_at" && suffix != "remove_swap";
}

bool isRemovedVectorCompatibilityHelper(const std::string &helperName) {
  return helperName == "count" || helperName == "capacity" || helperName == "at" || helperName == "at_unsafe" ||
         helperName == "push" || helperName == "pop" || helperName == "reserve" || helperName == "clear" ||
         helperName == "remove_at" || helperName == "remove_swap";
}

bool resolveVectorHelperAliasName(const Expr &expr, std::string &helperNameOut) {
  if (expr.name.empty()) {
    return false;
  }
  std::string normalized = expr.name;
  if (!normalized.empty() && normalized.front() == '/') {
    normalized.erase(0, 1);
  }
  const std::string vectorPrefix = "vector/";
  const std::string arrayPrefix = "array/";
  const std::string stdVectorPrefix = "std/collections/vector/";
  if (normalized.rfind(vectorPrefix, 0) == 0) {
    helperNameOut = normalized.substr(vectorPrefix.size());
    if (isRemovedVectorCompatibilityHelper(helperNameOut)) {
      return false;
    }
    return true;
  }
  if (normalized.rfind(arrayPrefix, 0) == 0) {
    helperNameOut = normalized.substr(arrayPrefix.size());
    if (isRemovedVectorCompatibilityHelper(helperNameOut)) {
      return false;
    }
    return true;
  }
  if (normalized.rfind(stdVectorPrefix, 0) == 0) {
    helperNameOut = normalized.substr(stdVectorPrefix.size());
    return true;
  }
  return false;
}

bool resolveMapHelperAliasName(const Expr &expr, std::string &helperNameOut) {
  if (expr.name.empty()) {
    return false;
  }
  std::string normalized = expr.name;
  if (!normalized.empty() && normalized.front() == '/') {
    normalized.erase(0, 1);
  }
  const std::string mapPrefix = "map/";
  const std::string stdMapPrefix = "std/collections/map/";
  if (normalized.rfind(mapPrefix, 0) == 0) {
    helperNameOut = normalized.substr(mapPrefix.size());
    return true;
  }
  if (normalized.rfind(stdMapPrefix, 0) == 0) {
    helperNameOut = normalized.substr(stdMapPrefix.size());
    return true;
  }
  return false;
}

std::string normalizeCollectionHelperPath(const std::string &path);

std::string normalizeCollectionHelperPath(const std::string &path) {
  std::string normalizedPath = path;
  if (!normalizedPath.empty() && normalizedPath.front() != '/') {
    if (normalizedPath.rfind("array/", 0) == 0 || normalizedPath.rfind("vector/", 0) == 0 ||
        normalizedPath.rfind("std/collections/vector/", 0) == 0 || normalizedPath.rfind("map/", 0) == 0 ||
        normalizedPath.rfind("std/collections/map/", 0) == 0) {
      normalizedPath.insert(normalizedPath.begin(), '/');
    }
  }
  return normalizedPath;
}

bool isExplicitRemovedVectorMethodAliasPath(const std::string &methodName) {
  if (methodName.empty()) {
    return false;
  }
  std::string normalized = methodName;
  if (!normalized.empty() && normalized.front() == '/') {
    normalized.erase(normalized.begin());
  }
  const std::string vectorPrefix = "vector/";
  const std::string arrayPrefix = "array/";
  const std::string stdVectorPrefix = "std/collections/vector/";
  if (normalized.rfind(vectorPrefix, 0) == 0) {
    return isRemovedVectorCompatibilityHelper(normalized.substr(vectorPrefix.size()));
  }
  if (normalized.rfind(arrayPrefix, 0) == 0) {
    return isRemovedVectorCompatibilityHelper(normalized.substr(arrayPrefix.size()));
  }
  if (normalized.rfind(stdVectorPrefix, 0) == 0) {
    return isRemovedVectorCompatibilityHelper(normalized.substr(stdVectorPrefix.size()));
  }
  return false;
}

bool isExplicitMapMethodAliasPath(const std::string &methodName) {
  if (methodName.empty()) {
    return false;
  }
  std::string normalized = methodName;
  if (!normalized.empty() && normalized.front() == '/') {
    normalized.erase(normalized.begin());
  }
  const std::string mapPrefix = "map/";
  const std::string stdMapPrefix = "std/collections/map/";
  auto isBuiltinMapHelper = [](const std::string &helperName) {
    return helperName == "count" || helperName == "at" || helperName == "at_unsafe";
  };
  if (normalized.rfind(mapPrefix, 0) == 0) {
    return isBuiltinMapHelper(normalized.substr(mapPrefix.size()));
  }
  if (normalized.rfind(stdMapPrefix, 0) == 0) {
    return isBuiltinMapHelper(normalized.substr(stdMapPrefix.size()));
  }
  return false;
}

bool isExplicitMapContainsOrTryAtCompatibilityMethodAliasPath(const std::string &methodName) {
  if (methodName.empty()) {
    return false;
  }
  std::string normalized = methodName;
  if (!normalized.empty() && normalized.front() == '/') {
    normalized.erase(normalized.begin());
  }
  const std::string mapPrefix = "map/";
  auto isRemovedCompatibilityMethod = [](const std::string &helperName) {
    return helperName == "contains" || helperName == "tryAt";
  };
  if (normalized.rfind(mapPrefix, 0) == 0) {
    return isRemovedCompatibilityMethod(normalized.substr(mapPrefix.size()));
  }
  return false;
}

bool isVectorBuiltinName(const Expr &expr, const char *name) {
  if (isSimpleCallName(expr, name)) {
    return true;
  }
  std::string aliasName;
  return resolveVectorHelperAliasName(expr, aliasName) && aliasName == name;
}

bool isMapBuiltinName(const Expr &expr, const char *name) {
  if (isSimpleCallName(expr, name)) {
    return true;
  }
  std::string aliasName;
  return resolveMapHelperAliasName(expr, aliasName) && aliasName == name;
}

bool isExplicitMapHelperFallbackPath(const Expr &expr) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty() || expr.isMethodCall) {
    return false;
  }
  const std::string normalizedPath = normalizeCollectionHelperPath(expr.name);
  return normalizedPath == "/map/count" || normalizedPath == "/map/at" || normalizedPath == "/map/at_unsafe" ||
         normalizedPath == "/std/collections/map/count" || normalizedPath == "/std/collections/map/at" ||
         normalizedPath == "/std/collections/map/at_unsafe";
}

bool isExplicitVectorAccessHelperPath(const std::string &path) {
  const std::string normalizedPath = normalizeCollectionHelperPath(path);
  return normalizedPath == "/vector/at" || normalizedPath == "/vector/at_unsafe" ||
         normalizedPath == "/std/collections/vector/at" ||
         normalizedPath == "/std/collections/vector/at_unsafe";
}

bool isExplicitVectorAccessHelperExpr(const Expr &expr) {
  return expr.kind == Expr::Kind::Call && !expr.name.empty() &&
         isExplicitVectorAccessHelperPath(expr.name);
}

std::vector<std::string> collectionHelperPathCandidates(const std::string &path);

bool isBuiltinMapHelperSuffix(const std::string &suffix) {
  return suffix == "count" || suffix == "contains" || suffix == "tryAt" || suffix == "at" ||
         suffix == "at_unsafe";
}

void pruneRemovedMapCompatibilityCallReturnCandidates(std::vector<std::string> &candidates,
                                                      const std::string &path) {
  const std::string normalizedPath = normalizeCollectionHelperPath(path);
  if (normalizedPath.rfind("/map/", 0) != 0) {
    return;
  }

  const std::string suffix = normalizedPath.substr(std::string("/map/").size());
  if (!isBuiltinMapHelperSuffix(suffix)) {
    return;
  }

  const std::string canonicalCandidate = "/std/collections/map/" + suffix;
  for (auto it = candidates.begin(); it != candidates.end();) {
    if (*it == canonicalCandidate) {
      it = candidates.erase(it);
    } else {
      ++it;
    }
  }
}

void pruneRemovedVectorCompatibilityCallReturnCandidates(std::vector<std::string> &candidates,
                                                         const std::string &path) {
  const std::string normalizedPath = normalizeCollectionHelperPath(path);
  if (normalizedPath.rfind("/vector/", 0) != 0) {
    return;
  }

  const std::string suffix = normalizedPath.substr(std::string("/vector/").size());
  if (suffix != "at" && suffix != "at_unsafe") {
    return;
  }

  const std::string canonicalCandidate = "/std/collections/vector/" + suffix;
  for (auto it = candidates.begin(); it != candidates.end();) {
    if (*it == canonicalCandidate) {
      it = candidates.erase(it);
    } else {
      ++it;
    }
  }
}

bool isAllowedResolvedMapDirectCallPath(const std::string &callPath, const std::string &resolvedPath) {
  if (!isExplicitMapMethodAliasPath(callPath)) {
    return true;
  }

  auto allowedCandidates = collectionHelperPathCandidates(callPath);
  pruneRemovedMapCompatibilityCallReturnCandidates(allowedCandidates, callPath);
  const std::string normalizedResolvedPath = normalizeCollectionHelperPath(resolvedPath);
  return std::any_of(allowedCandidates.begin(), allowedCandidates.end(), [&](const std::string &candidate) {
    return candidate == normalizedResolvedPath;
  });
}

bool isAllowedResolvedVectorDirectCallPath(const std::string &callPath, const std::string &resolvedPath) {
  const std::string normalizedCallPath = normalizeCollectionHelperPath(callPath);
  if (normalizedCallPath.rfind("/vector/", 0) != 0 &&
      normalizedCallPath.rfind("/std/collections/vector/", 0) != 0) {
    return true;
  }
  auto allowedCandidates = collectionHelperPathCandidates(callPath);
  pruneRemovedVectorCompatibilityCallReturnCandidates(allowedCandidates, callPath);
  const std::string normalizedResolvedPath = normalizeCollectionHelperPath(resolvedPath);
  return std::any_of(allowedCandidates.begin(), allowedCandidates.end(), [&](const std::string &candidate) {
    return candidate == normalizedResolvedPath;
  });
}

std::string normalizeMapImportAliasPath(const std::string &path) {
  if (path.empty() || path.front() == '/') {
    return path;
  }
  if (path.rfind("map/", 0) == 0 || path.rfind("std/collections/map/", 0) == 0) {
    return "/" + path;
  }
  return path;
}

std::vector<std::string> collectionHelperPathCandidates(const std::string &path) {
  std::vector<std::string> candidates;
  auto appendUnique = [&](const std::string &candidate) {
    for (const auto &existing : candidates) {
      if (existing == candidate) {
        return;
      }
    }
    candidates.push_back(candidate);
  };

  const std::string normalizedPath = normalizeCollectionHelperPath(path);

  appendUnique(path);
  appendUnique(normalizedPath);
  if (normalizedPath.rfind("/array/", 0) == 0) {
    const std::string suffix = normalizedPath.substr(std::string("/array/").size());
    if (allowsArrayVectorCompatibilitySuffix(suffix)) {
      appendUnique("/vector/" + suffix);
      appendUnique("/std/collections/vector/" + suffix);
    }
  } else if (normalizedPath.rfind("/vector/", 0) == 0) {
    const std::string suffix = normalizedPath.substr(std::string("/vector/").size());
    if (allowsVectorStdlibCompatibilitySuffix(suffix)) {
      appendUnique("/std/collections/vector/" + suffix);
    }
    if (allowsArrayVectorCompatibilitySuffix(suffix)) {
      appendUnique("/array/" + suffix);
    }
  } else if (normalizedPath.rfind("/std/collections/vector/", 0) == 0) {
    const std::string suffix = normalizedPath.substr(std::string("/std/collections/vector/").size());
    if (allowsVectorStdlibCompatibilitySuffix(suffix)) {
      appendUnique("/vector/" + suffix);
    }
    if (allowsArrayVectorCompatibilitySuffix(suffix)) {
      appendUnique("/array/" + suffix);
    }
  } else if (normalizedPath.rfind("/map/", 0) == 0) {
    appendUnique("/std/collections/map/" + normalizedPath.substr(std::string("/map/").size()));
  } else if (normalizedPath.rfind("/std/collections/map/", 0) == 0) {
    const std::string suffix = normalizedPath.substr(std::string("/std/collections/map/").size());
    if (suffix != "map" && suffix != "count" && suffix != "contains" && suffix != "tryAt" &&
        suffix != "at" && suffix != "at_unsafe") {
      appendUnique("/map/" + suffix);
    }
  }

  return candidates;
}

} // namespace

SetupTypeAdapters makeSetupTypeAdapters() {
  SetupTypeAdapters adapters;
  adapters.valueKindFromTypeName = makeValueKindFromTypeName();
  adapters.combineNumericKinds = makeCombineNumericKinds();
  return adapters;
}

ValueKindFromTypeNameFn makeValueKindFromTypeName() {
  return [](const std::string &name) {
    return valueKindFromTypeName(name);
  };
}

CombineNumericKindsFn makeCombineNumericKinds() {
  return [](LocalInfo::ValueKind left, LocalInfo::ValueKind right) {
    return combineNumericKinds(left, right);
  };
}

bool getNamespacedCollectionHelperName(const Expr &expr, std::string &collectionOut, std::string &helperOut) {
  collectionOut.clear();
  helperOut.clear();

  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }

  if (resolveVectorHelperAliasName(expr, helperOut)) {
    collectionOut = "vector";
    return true;
  }
  if (resolveMapHelperAliasName(expr, helperOut)) {
    collectionOut = "map";
    return true;
  }
  return false;
}

LocalInfo::ValueKind valueKindFromTypeName(const std::string &name) {
  if (name == "int" || name == "i32") {
    return LocalInfo::ValueKind::Int32;
  }
  if (name == "i64") {
    return LocalInfo::ValueKind::Int64;
  }
  if (name == "u64") {
    return LocalInfo::ValueKind::UInt64;
  }
  if (name == "float" || name == "f32") {
    return LocalInfo::ValueKind::Float32;
  }
  if (name == "f64") {
    return LocalInfo::ValueKind::Float64;
  }
  if (name == "bool") {
    return LocalInfo::ValueKind::Bool;
  }
  if (name == "string") {
    return LocalInfo::ValueKind::String;
  }
  if (name == "FileError") {
    return LocalInfo::ValueKind::Int32;
  }

  std::string base;
  std::string arg;
  if (splitTemplateTypeName(name, base, arg) && base == "File") {
    return LocalInfo::ValueKind::Int64;
  }
  return LocalInfo::ValueKind::Unknown;
}

LocalInfo::ValueKind combineNumericKinds(LocalInfo::ValueKind left, LocalInfo::ValueKind right) {
  if (left == LocalInfo::ValueKind::Unknown || right == LocalInfo::ValueKind::Unknown) {
    return LocalInfo::ValueKind::Unknown;
  }
  if (left == LocalInfo::ValueKind::String || right == LocalInfo::ValueKind::String) {
    return LocalInfo::ValueKind::Unknown;
  }
  if (left == LocalInfo::ValueKind::Bool || right == LocalInfo::ValueKind::Bool) {
    return LocalInfo::ValueKind::Unknown;
  }
  if (left == LocalInfo::ValueKind::Float32 || right == LocalInfo::ValueKind::Float32 ||
      left == LocalInfo::ValueKind::Float64 || right == LocalInfo::ValueKind::Float64) {
    if (left == LocalInfo::ValueKind::Float32 && right == LocalInfo::ValueKind::Float32) {
      return LocalInfo::ValueKind::Float32;
    }
    if (left == LocalInfo::ValueKind::Float64 && right == LocalInfo::ValueKind::Float64) {
      return LocalInfo::ValueKind::Float64;
    }
    return LocalInfo::ValueKind::Unknown;
  }
  if (left == LocalInfo::ValueKind::UInt64 || right == LocalInfo::ValueKind::UInt64) {
    return (left == LocalInfo::ValueKind::UInt64 && right == LocalInfo::ValueKind::UInt64)
               ? LocalInfo::ValueKind::UInt64
               : LocalInfo::ValueKind::Unknown;
  }
  if (left == LocalInfo::ValueKind::Int64 || right == LocalInfo::ValueKind::Int64) {
    if ((left == LocalInfo::ValueKind::Int64 || left == LocalInfo::ValueKind::Int32) &&
        (right == LocalInfo::ValueKind::Int64 || right == LocalInfo::ValueKind::Int32)) {
      return LocalInfo::ValueKind::Int64;
    }
    return LocalInfo::ValueKind::Unknown;
  }
  if (left == LocalInfo::ValueKind::Int32 && right == LocalInfo::ValueKind::Int32) {
    return LocalInfo::ValueKind::Int32;
  }
  return LocalInfo::ValueKind::Unknown;
}

LocalInfo::ValueKind comparisonKind(LocalInfo::ValueKind left, LocalInfo::ValueKind right) {
  if (left == LocalInfo::ValueKind::Bool) {
    left = LocalInfo::ValueKind::Int32;
  }
  if (right == LocalInfo::ValueKind::Bool) {
    right = LocalInfo::ValueKind::Int32;
  }
  return combineNumericKinds(left, right);
}

std::string typeNameForValueKind(LocalInfo::ValueKind kind) {
  switch (kind) {
    case LocalInfo::ValueKind::Int32:
      return "i32";
    case LocalInfo::ValueKind::Int64:
      return "i64";
    case LocalInfo::ValueKind::UInt64:
      return "u64";
    case LocalInfo::ValueKind::Float32:
      return "f32";
    case LocalInfo::ValueKind::Float64:
      return "f64";
    case LocalInfo::ValueKind::Bool:
      return "bool";
    case LocalInfo::ValueKind::String:
      return "string";
    default:
      return "";
  }
}

std::string normalizeDeclaredCollectionTypeBase(const std::string &base) {
  if (base == "/map" || base == "std/collections/map" || base == "/std/collections/map") {
    return "map";
  }
  return base;
}

bool inferDeclaredReturnCollection(const Definition &definition,
                                   std::string &collectionNameOut,
                                   std::vector<std::string> &collectionArgsOut) {
  collectionNameOut.clear();
  collectionArgsOut.clear();
  auto inferCollectionFromType = [&](const std::string &typeName,
                                     auto &&inferCollectionFromTypeRef) -> bool {
    std::string base;
    std::string argText;
    if (!splitTemplateTypeName(typeName, base, argText)) {
      return false;
    }
    base = normalizeDeclaredCollectionTypeBase(base);
    std::vector<std::string> args;
    if (!splitTemplateArgs(argText, args)) {
      return false;
    }
    if ((base == "array" || base == "vector" || base == "soa_vector") && args.size() == 1) {
      collectionNameOut = base;
      collectionArgsOut = std::move(args);
      return true;
    }
    if (base == "map" && args.size() == 2) {
      collectionNameOut = base;
      collectionArgsOut = std::move(args);
      return true;
    }
    if ((base == "Reference" || base == "Pointer") && args.size() == 1) {
      return inferCollectionFromTypeRef(trimTemplateTypeText(args.front()), inferCollectionFromTypeRef);
    }
    return false;
  };
  for (const auto &transform : definition.transforms) {
    if (transform.name != "return" || transform.templateArgs.size() != 1) {
      continue;
    }
    const std::string declaredType = trimTemplateTypeText(transform.templateArgs.front());
    if (declaredType == "auto") {
      break;
    }
    return inferCollectionFromType(declaredType, inferCollectionFromType);
  }
  auto isEnvelopeValueExpr = [&](const Expr &candidate, bool allowAnyName) -> bool {
    if (candidate.kind != Expr::Kind::Call || candidate.isBinding || candidate.isMethodCall) {
      return false;
    }
    if (!candidate.args.empty() || !candidate.templateArgs.empty() || hasNamedArguments(candidate.argNames)) {
      return false;
    }
    if (!candidate.hasBodyArguments && candidate.bodyArguments.empty()) {
      return false;
    }
    return allowAnyName || isBlockCall(candidate);
  };
  auto getEnvelopeValueExpr = [&](const Expr &candidate, bool allowAnyName) -> const Expr * {
    if (!isEnvelopeValueExpr(candidate, allowAnyName)) {
      return nullptr;
    }
    const Expr *valueExpr = nullptr;
    for (const auto &bodyExpr : candidate.bodyArguments) {
      if (bodyExpr.isBinding) {
        continue;
      }
      valueExpr = &bodyExpr;
    }
    return valueExpr;
  };
  auto inferDirectCollectionCall = [&](const Expr &candidate,
                                       std::string &nameOut,
                                       std::vector<std::string> &argsOut) -> bool {
    nameOut.clear();
    argsOut.clear();
    if (candidate.kind != Expr::Kind::Call || candidate.isBinding || candidate.isMethodCall) {
      return false;
    }
    std::string collection;
    if (getBuiltinCollectionName(candidate, collection)) {
      if ((collection == "array" || collection == "vector" || collection == "soa_vector") &&
          candidate.templateArgs.size() == 1) {
        nameOut = collection;
        argsOut = candidate.templateArgs;
        return true;
      }
      if (collection == "map" && candidate.templateArgs.size() == 2) {
        nameOut = "map";
        argsOut = candidate.templateArgs;
        return true;
      }
    }
    auto normalizedName = candidate.name;
    if (!normalizedName.empty() && normalizedName.front() == '/') {
      normalizedName.erase(normalizedName.begin());
    }
    auto matchesPath = [&](std::string_view basePath) {
      return normalizedName == basePath || normalizedName.rfind(std::string(basePath) + "__", 0) == 0;
    };
    auto isDirectMapConstructor = [&]() {
      return matchesPath("std/collections/map/map") ||
             matchesPath("std/collections/mapNew") ||
             matchesPath("std/collections/mapSingle") ||
             matchesPath("std/collections/mapDouble") ||
             matchesPath("std/collections/mapPair") ||
             matchesPath("std/collections/mapTriple") ||
             matchesPath("std/collections/mapQuad") ||
             matchesPath("std/collections/mapQuint") ||
             matchesPath("std/collections/mapSext") ||
             matchesPath("std/collections/mapSept") ||
             matchesPath("std/collections/mapOct") ||
             matchesPath("std/collections/experimental_map/mapNew") ||
             matchesPath("std/collections/experimental_map/mapSingle") ||
             matchesPath("std/collections/experimental_map/mapDouble") ||
             matchesPath("std/collections/experimental_map/mapPair") ||
             matchesPath("std/collections/experimental_map/mapTriple") ||
             matchesPath("std/collections/experimental_map/mapQuad") ||
             matchesPath("std/collections/experimental_map/mapQuint") ||
             matchesPath("std/collections/experimental_map/mapSext") ||
             matchesPath("std/collections/experimental_map/mapSept") ||
             matchesPath("std/collections/experimental_map/mapOct") ||
             isSimpleCallName(candidate, "mapNew") ||
             isSimpleCallName(candidate, "mapSingle") ||
             isSimpleCallName(candidate, "mapDouble") ||
             isSimpleCallName(candidate, "mapPair") ||
             isSimpleCallName(candidate, "mapTriple") ||
             isSimpleCallName(candidate, "mapQuad") ||
             isSimpleCallName(candidate, "mapQuint") ||
             isSimpleCallName(candidate, "mapSext") ||
             isSimpleCallName(candidate, "mapSept") ||
             isSimpleCallName(candidate, "mapOct");
    };
    auto isDirectVectorConstructor = [&]() {
      return matchesPath("std/collections/vector/vector") ||
             matchesPath("std/collections/vectorNew") ||
             matchesPath("std/collections/vectorSingle") ||
             isSimpleCallName(candidate, "vectorNew") ||
             isSimpleCallName(candidate, "vectorSingle");
    };
    if (isDirectMapConstructor() && candidate.templateArgs.size() == 2) {
      nameOut = "map";
      argsOut = candidate.templateArgs;
      return true;
    }
    if (isDirectMapConstructor() && candidate.args.size() % 2 == 0 && !candidate.args.empty()) {
      auto inferLiteralType = [&](const Expr &value, std::string &typeOut) -> bool {
        typeOut.clear();
        if (value.kind == Expr::Kind::Literal) {
          typeOut = value.isUnsigned ? "u64" : (value.intWidth == 64 ? "i64" : "i32");
          return true;
        }
        if (value.kind == Expr::Kind::BoolLiteral) {
          typeOut = "bool";
          return true;
        }
        if (value.kind == Expr::Kind::FloatLiteral) {
          typeOut = value.floatWidth == 64 ? "f64" : "f32";
          return true;
        }
        if (value.kind == Expr::Kind::StringLiteral) {
          typeOut = "string";
          return true;
        }
        return false;
      };
      std::string keyType;
      std::string valueType;
      for (size_t i = 0; i < candidate.args.size(); i += 2) {
        std::string currentKeyType;
        std::string currentValueType;
        if (!inferLiteralType(candidate.args[i], currentKeyType) ||
            !inferLiteralType(candidate.args[i + 1], currentValueType)) {
          return false;
        }
        if (keyType.empty()) {
          keyType = currentKeyType;
        } else if (keyType != currentKeyType) {
          return false;
        }
        if (valueType.empty()) {
          valueType = currentValueType;
        } else if (valueType != currentValueType) {
          return false;
        }
      }
      nameOut = "map";
      argsOut = {keyType, valueType};
      return true;
    }
    if (isDirectVectorConstructor() && candidate.templateArgs.size() == 1) {
      nameOut = "vector";
      argsOut = candidate.templateArgs;
      return true;
    }
    return false;
  };
  std::function<bool(const Expr &)> inferCollectionFromExpr;
  inferCollectionFromExpr = [&](const Expr &candidate) -> bool {
    if (isIfCall(candidate) && candidate.args.size() == 3) {
      const Expr &thenArg = candidate.args[1];
      const Expr &elseArg = candidate.args[2];
      const Expr *thenValue = getEnvelopeValueExpr(thenArg, true);
      const Expr *elseValue = getEnvelopeValueExpr(elseArg, true);
      std::string thenName;
      std::vector<std::string> thenArgs;
      std::string elseName;
      std::vector<std::string> elseArgs;
      if (!inferCollectionFromExpr(thenValue ? *thenValue : thenArg)) {
        return false;
      }
      thenName = collectionNameOut;
      thenArgs = collectionArgsOut;
      if (!inferCollectionFromExpr(elseValue ? *elseValue : elseArg)) {
        return false;
      }
      elseName = collectionNameOut;
      elseArgs = collectionArgsOut;
      if (thenName != elseName || thenArgs != elseArgs) {
        return false;
      }
      collectionNameOut = std::move(thenName);
      collectionArgsOut = std::move(thenArgs);
      return true;
    }
    if (const Expr *valueExpr = getEnvelopeValueExpr(candidate, false)) {
      if (isReturnCall(*valueExpr) && !valueExpr->args.empty()) {
        return inferCollectionFromExpr(valueExpr->args.front());
      }
      return inferCollectionFromExpr(*valueExpr);
    }
    return inferDirectCollectionCall(candidate, collectionNameOut, collectionArgsOut);
  };
  const Expr *valueExpr = nullptr;
  bool sawReturn = false;
  for (const auto &stmt : definition.statements) {
    if (stmt.isBinding) {
      continue;
    }
    if (isReturnCall(stmt)) {
      if (stmt.args.size() != 1) {
        return false;
      }
      valueExpr = &stmt.args.front();
      sawReturn = true;
      continue;
    }
    if (!sawReturn) {
      valueExpr = &stmt;
    }
  }
  if (definition.returnExpr.has_value()) {
    valueExpr = &*definition.returnExpr;
  }
  if (valueExpr != nullptr && inferCollectionFromExpr(*valueExpr)) {
    return true;
  }
  return false;
}

bool inferReceiverTypeFromDeclaredReturn(const Definition &definition, std::string &typeNameOut) {
  std::vector<std::string> collectionArgs;
  if (inferDeclaredReturnCollection(definition, typeNameOut, collectionArgs)) {
    return true;
  }

  for (const auto &transform : definition.transforms) {
    if (transform.name != "return" || transform.templateArgs.size() != 1) {
      continue;
    }
    const std::string &declaredType = transform.templateArgs.front();
    if (declaredType.empty() || declaredType == "void" || declaredType == "auto") {
      return false;
    }
    std::string base;
    std::string argText;
    if (splitTemplateTypeName(declaredType, base, argText)) {
      // Non-collection templated returns are not method receiver targets.
      return false;
    }
    typeNameOut = declaredType;
    if (!typeNameOut.empty() && typeNameOut.front() == '/') {
      typeNameOut.erase(0, 1);
    }
    return !typeNameOut.empty();
  }
  return false;
}

bool resolveMethodCallReceiverExpr(const Expr &callExpr,
                                   const LocalMap &localsIn,
                                   const IsMethodCallClassifierFn &isArrayCountCall,
                                   const IsMethodCallClassifierFn &isVectorCapacityCall,
                                   const IsMethodCallClassifierFn &isEntryArgsName,
                                   const Expr *&receiverOut,
                                   std::string &errorOut) {
  receiverOut = nullptr;

  if (callExpr.kind != Expr::Kind::Call || callExpr.isBinding || !callExpr.isMethodCall) {
    return false;
  }
  if (callExpr.args.empty()) {
    errorOut = "method call missing receiver";
    return false;
  }
  std::string accessName;
  const bool isBuiltinAccessCall = getBuiltinArrayAccessName(callExpr, accessName) && callExpr.args.size() == 2;
  const bool isBuiltinCountOrCapacityCall =
      isVectorBuiltinName(callExpr, "count") || isMapBuiltinName(callExpr, "count") ||
      isVectorBuiltinName(callExpr, "capacity");
  const bool isBuiltinBareVectorCapacityMethod =
      isSimpleCallName(callExpr, "capacity") &&
      isVectorCapacityCall && isVectorCapacityCall(callExpr, localsIn);
  const bool isBuiltinVectorMutatorCall =
      isVectorBuiltinName(callExpr, "push") || isVectorBuiltinName(callExpr, "pop") ||
      isVectorBuiltinName(callExpr, "reserve") || isVectorBuiltinName(callExpr, "clear") ||
      isVectorBuiltinName(callExpr, "remove_at") || isVectorBuiltinName(callExpr, "remove_swap");
  const bool isExplicitRemovedVectorMethodAlias = isExplicitRemovedVectorMethodAliasPath(callExpr.name);
  const bool isExplicitMapMethodAlias = isExplicitMapMethodAliasPath(callExpr.name);
  const bool isBuiltinMapContainsOrTryAtCall =
      isSimpleCallName(callExpr, "contains") || isSimpleCallName(callExpr, "tryAt");
  const bool allowBuiltinFallback =
      !isExplicitRemovedVectorMethodAlias && !isExplicitMapMethodAlias &&
      !isBuiltinBareVectorCapacityMethod &&
      (isBuiltinCountOrCapacityCall || isBuiltinVectorMutatorCall ||
       isBuiltinMapContainsOrTryAtCall ||
       (isArrayCountCall && isArrayCountCall(callExpr, localsIn)) ||
       (isVectorCapacityCall && isVectorCapacityCall(callExpr, localsIn)) || isBuiltinAccessCall);
  const Expr &receiver = callExpr.args.front();
  if (isEntryArgsName && isEntryArgsName(receiver, localsIn)) {
    if (allowBuiltinFallback) {
      return false;
    }
    errorOut = "unknown method target for " + callExpr.name;
    return false;
  }

  receiverOut = &receiver;
  return true;
}

bool resolveMethodReceiverTypeFromLocalInfo(const LocalInfo &localInfo,
                                            std::string &typeNameOut,
                                            std::string &resolvedTypePathOut) {
  typeNameOut.clear();
  resolvedTypePathOut.clear();

  if (localInfo.kind == LocalInfo::Kind::Array) {
    if (!localInfo.structTypeName.empty()) {
      resolvedTypePathOut = localInfo.structTypeName;
    } else {
      typeNameOut = "array";
    }
    return true;
  }
  if (localInfo.kind == LocalInfo::Kind::Vector) {
    typeNameOut = "vector";
    return true;
  }
  if (localInfo.kind == LocalInfo::Kind::Map) {
    typeNameOut = "map";
    return true;
  }
  if (localInfo.isSoaVector) {
    typeNameOut = "soa_vector";
    return true;
  }
  if (localInfo.kind == LocalInfo::Kind::Reference &&
      (localInfo.referenceToArray || localInfo.referenceToVector || localInfo.referenceToMap)) {
    if (!localInfo.structTypeName.empty()) {
      resolvedTypePathOut = localInfo.structTypeName;
    } else {
      typeNameOut = localInfo.referenceToMap ? "map" : (localInfo.referenceToVector ? "vector" : "array");
    }
    return true;
  }
  if (localInfo.kind == LocalInfo::Kind::Pointer && localInfo.pointerToArray) {
    typeNameOut = "array";
    return true;
  }
  if (localInfo.kind == LocalInfo::Kind::Pointer && localInfo.pointerToVector) {
    typeNameOut = "vector";
    return true;
  }
  if (localInfo.kind == LocalInfo::Kind::Pointer && localInfo.pointerToMap) {
    typeNameOut = "map";
    return true;
  }
  if (localInfo.kind == LocalInfo::Kind::Pointer || localInfo.kind == LocalInfo::Kind::Reference) {
    return false;
  }
  if (localInfo.kind == LocalInfo::Kind::Value && !localInfo.structTypeName.empty()) {
    resolvedTypePathOut = localInfo.structTypeName;
    return true;
  }

  typeNameOut = typeNameForValueKind(localInfo.valueKind);
  return true;
}

std::string resolveMethodReceiverTypeNameFromCallExpr(const Expr &receiverCallExpr,
                                                      LocalInfo::ValueKind inferredKind) {
  std::string collection;
  if (getBuiltinCollectionName(receiverCallExpr, collection)) {
    if (collection == "array" && receiverCallExpr.templateArgs.size() == 1) {
      return "array";
    }
    if (collection == "vector" && receiverCallExpr.templateArgs.size() == 1) {
      return "vector";
    }
    if (collection == "map" && receiverCallExpr.templateArgs.size() == 2) {
      return "map";
    }
    if (collection == "soa_vector" && receiverCallExpr.templateArgs.size() == 1) {
      return "soa_vector";
    }
  }
  return typeNameForValueKind(inferredKind);
}

bool inferBuiltinAccessReceiverResultKind(const Expr &receiverCallExpr,
                                          const LocalMap &localsIn,
                                          const InferReceiverExprKindFn &inferExprKind,
                                          const ResolveReceiverExprPathFn &resolveExprPath,
                                          const GetReturnInfoForPathFn &getReturnInfo,
                                          const std::unordered_map<std::string, const Definition *> &defMap,
                                          LocalInfo::ValueKind &kindOut) {
  kindOut = LocalInfo::ValueKind::Unknown;
  if ((receiverCallExpr.isMethodCall && isExplicitMapMethodAliasPath(receiverCallExpr.name)) ||
      isExplicitMapHelperFallbackPath(receiverCallExpr) ||
      isExplicitVectorAccessHelperExpr(receiverCallExpr)) {
    return false;
  }

  std::string accessName;
  if (!(receiverCallExpr.kind == Expr::Kind::Call && getBuiltinArrayAccessName(receiverCallExpr, accessName) &&
        receiverCallExpr.args.size() == 2)) {
    return false;
  }

  const Expr &accessReceiver = receiverCallExpr.args.front();
  auto assignKind = [&](LocalInfo::ValueKind valueKind) {
    if (valueKind == LocalInfo::ValueKind::Unknown) {
      return false;
    }
    kindOut = valueKind;
    return true;
  };
  auto assignCollectionKind = [&](const std::string &collectionName, const std::vector<std::string> &collectionArgs) {
    if ((collectionName == "vector" || collectionName == "array") && collectionArgs.size() == 1) {
      return assignKind(valueKindFromTypeName(collectionArgs.front()));
    }
    if (collectionName == "map" && collectionArgs.size() == 2) {
      return assignKind(valueKindFromTypeName(collectionArgs.back()));
    }
    return false;
  };

  if (accessReceiver.kind == Expr::Kind::Name) {
    auto localIt = localsIn.find(accessReceiver.name);
    if (localIt == localsIn.end()) {
      return false;
    }
    const LocalInfo &receiverInfo = localIt->second;
    if (receiverInfo.kind == LocalInfo::Kind::Vector || receiverInfo.kind == LocalInfo::Kind::Array ||
        (receiverInfo.kind == LocalInfo::Kind::Reference &&
         (receiverInfo.referenceToArray || receiverInfo.referenceToVector)) ||
        (receiverInfo.kind == LocalInfo::Kind::Pointer && receiverInfo.pointerToArray) ||
        (receiverInfo.kind == LocalInfo::Kind::Pointer && receiverInfo.pointerToVector)) {
      return assignKind(receiverInfo.valueKind);
    }
    if (receiverInfo.kind == LocalInfo::Kind::Map ||
        (receiverInfo.kind == LocalInfo::Kind::Reference && receiverInfo.referenceToMap) ||
        (receiverInfo.kind == LocalInfo::Kind::Pointer && receiverInfo.pointerToMap)) {
      return assignKind(receiverInfo.mapValueKind);
    }
    if (receiverInfo.kind == LocalInfo::Kind::Value && receiverInfo.valueKind == LocalInfo::ValueKind::String) {
      return assignKind(LocalInfo::ValueKind::Int32);
    }
    return false;
  }

  if (accessReceiver.kind != Expr::Kind::Call) {
    return false;
  }

  std::string collectionName;
  if (getBuiltinCollectionName(accessReceiver, collectionName)) {
    if (collectionName == "string") {
      return assignKind(LocalInfo::ValueKind::Int32);
    }
    if (assignCollectionKind(collectionName, accessReceiver.templateArgs)) {
      return true;
    }
  }

  if (inferExprKind && inferExprKind(accessReceiver, localsIn) == LocalInfo::ValueKind::String) {
    return assignKind(LocalInfo::ValueKind::Int32);
  }

  const std::string receiverPath = resolveExprPath ? resolveExprPath(accessReceiver) : std::string();
  if (receiverPath.empty()) {
    return false;
  }

  auto defIt = defMap.find(receiverPath);
  if (defIt != defMap.end() && defIt->second != nullptr) {
    std::string declaredCollectionName;
    std::vector<std::string> declaredCollectionArgs;
    if (inferDeclaredReturnCollection(*defIt->second, declaredCollectionName, declaredCollectionArgs) &&
        assignCollectionKind(declaredCollectionName, declaredCollectionArgs)) {
      return true;
    }
  }

  ReturnInfo receiverInfo;
  if (getReturnInfo && getReturnInfo(receiverPath, receiverInfo) && !receiverInfo.returnsVoid && !receiverInfo.returnsArray) {
    if (receiverInfo.kind == LocalInfo::ValueKind::String) {
      return assignKind(LocalInfo::ValueKind::Int32);
    }
    return assignKind(receiverInfo.kind);
  }

  return false;
}

bool isSoaVectorReceiverExpr(const Expr &receiverExpr, const LocalMap &localsIn) {
  if (receiverExpr.kind == Expr::Kind::Name) {
    auto it = localsIn.find(receiverExpr.name);
    return it != localsIn.end() && it->second.isSoaVector;
  }
  if (receiverExpr.kind == Expr::Kind::Call) {
    std::string collection;
    return getBuiltinCollectionName(receiverExpr, collection) && collection == "soa_vector";
  }
  return false;
}

std::string resolveMethodReceiverStructTypePathFromCallExpr(
    const Expr &receiverCallExpr,
    const std::string &resolvedReceiverPath,
    const std::unordered_map<std::string, std::string> &importAliases,
    const std::unordered_set<std::string> &structNames) {
  if (receiverCallExpr.isBinding || receiverCallExpr.isMethodCall) {
    return "";
  }

  auto resolveStructPathCandidates = [&](const std::string &path) -> std::string {
    const std::string normalizedPath = normalizeMapImportAliasPath(path);
    auto candidates = collectionHelperPathCandidates(normalizedPath);
    auto eraseCandidate = [&](const std::string &candidate) {
      for (auto it = candidates.begin(); it != candidates.end();) {
        if (*it == candidate) {
          it = candidates.erase(it);
        } else {
          ++it;
        }
      }
    };
    if (normalizedPath.rfind("/map/", 0) == 0) {
      const std::string suffix = normalizedPath.substr(std::string("/map/").size());
      if (suffix == "at" || suffix == "at_unsafe") {
        eraseCandidate("/std/collections/map/" + suffix);
      }
    } else if (normalizedPath.rfind("/std/collections/map/", 0) == 0) {
      const std::string suffix = normalizedPath.substr(std::string("/std/collections/map/").size());
      if (suffix == "at" || suffix == "at_unsafe") {
        eraseCandidate("/map/" + suffix);
      }
    }
    for (const auto &candidate : candidates) {
      if (structNames.count(candidate) > 0) {
        return candidate;
      }
    }
    return "";
  };

  std::string resolved = normalizeMapImportAliasPath(resolvedReceiverPath);
  auto importIt = importAliases.find(receiverCallExpr.name);
  std::string matched = resolveStructPathCandidates(resolved);
  if (!matched.empty()) {
    return matched;
  }
  if (importIt != importAliases.end()) {
    matched = resolveStructPathCandidates(importIt->second);
    if (!matched.empty()) {
      return matched;
    }
  }
  return "";
}

const Definition *resolveMethodDefinitionFromReceiverTarget(
    const std::string &methodName,
    const std::string &typeName,
    const std::string &resolvedTypePath,
    const std::unordered_map<std::string, const Definition *> &defMap,
    std::string &errorOut) {
  const bool isExplicitRemovedVectorMethodAlias = isExplicitRemovedVectorMethodAliasPath(methodName);
  const bool isExplicitMapMethodAlias = isExplicitMapMethodAliasPath(methodName);
  const bool isExplicitMapContainsOrTryAtCompatibilityMethodAlias =
      isExplicitMapContainsOrTryAtCompatibilityMethodAliasPath(methodName);
  std::string normalizedMethodName = methodName;
  if (!normalizedMethodName.empty() && normalizedMethodName.front() == '/') {
    normalizedMethodName.erase(normalizedMethodName.begin());
  }
  if (normalizedMethodName.rfind("vector/", 0) == 0) {
    normalizedMethodName = normalizedMethodName.substr(std::string("vector/").size());
  } else if (normalizedMethodName.rfind("array/", 0) == 0) {
    normalizedMethodName = normalizedMethodName.substr(std::string("array/").size());
  } else if (normalizedMethodName.rfind("std/collections/vector/", 0) == 0) {
    normalizedMethodName = normalizedMethodName.substr(std::string("std/collections/vector/").size());
  } else if (normalizedMethodName.rfind("map/", 0) == 0) {
    normalizedMethodName = normalizedMethodName.substr(std::string("map/").size());
  } else if (normalizedMethodName.rfind("std/collections/map/", 0) == 0) {
    normalizedMethodName = normalizedMethodName.substr(std::string("std/collections/map/").size());
  }
  std::string normalizedOriginalMethodName = methodName;
  if (!normalizedOriginalMethodName.empty() && normalizedOriginalMethodName.front() == '/') {
    normalizedOriginalMethodName.erase(normalizedOriginalMethodName.begin());
  }
  const bool isExplicitCanonicalMapMethodAlias =
      normalizedOriginalMethodName.rfind("std/collections/map/", 0) == 0;
  const bool isExplicitCompatibilityMapMethodAlias =
      isExplicitMapMethodAlias && !isExplicitCanonicalMapMethodAlias;
  std::string normalizedTypeName = typeName;
  if (!normalizedTypeName.empty() && normalizedTypeName.front() == '/') {
    normalizedTypeName.erase(normalizedTypeName.begin());
  }
  auto isVectorReceiverTarget = [&](const std::string &candidate) {
    return candidate == "vector" || candidate == "std/collections/vector";
  };
  auto isMapReceiverTarget = [&](const std::string &candidate) {
    return candidate == "map" || candidate == "std/collections/map";
  };
  auto shouldPreferCanonicalMapPath = [&](const std::string &candidate) {
    return !isExplicitCompatibilityMapMethodAlias && !isExplicitMapContainsOrTryAtCompatibilityMethodAlias &&
           isMapReceiverTarget(candidate) &&
           (isExplicitCanonicalMapMethodAlias || normalizedMethodName == "count" ||
            normalizedMethodName == "contains" || normalizedMethodName == "tryAt" ||
            normalizedMethodName == "at" || normalizedMethodName == "at_unsafe");
  };
  auto findMethodDefinitionByPath = [&](const std::string &path) -> const Definition * {
    auto defIt = defMap.find(path);
    if (defIt != defMap.end()) {
      return defIt->second;
    }
    if (path.rfind("/array/", 0) == 0) {
      const std::string suffix = path.substr(std::string("/array/").size());
      if (allowsArrayVectorCompatibilitySuffix(suffix)) {
        const std::string vectorAlias = "/vector/" + suffix;
        defIt = defMap.find(vectorAlias);
        if (defIt != defMap.end()) {
          return defIt->second;
        }
        const std::string stdlibAlias = "/std/collections/vector/" + suffix;
        defIt = defMap.find(stdlibAlias);
        if (defIt != defMap.end()) {
          return defIt->second;
        }
      }
    }
    if (path.rfind("/vector/", 0) == 0) {
      const std::string suffix = path.substr(std::string("/vector/").size());
      if (allowsVectorStdlibCompatibilitySuffix(suffix)) {
        const std::string stdlibAlias = "/std/collections/vector/" + suffix;
        defIt = defMap.find(stdlibAlias);
        if (defIt != defMap.end()) {
          return defIt->second;
        }
      }
      if (allowsArrayVectorCompatibilitySuffix(suffix)) {
        const std::string arrayAlias = "/array/" + suffix;
        defIt = defMap.find(arrayAlias);
        if (defIt != defMap.end()) {
          return defIt->second;
        }
      }
    }
    if (path.rfind("/std/collections/vector/", 0) == 0) {
      const std::string suffix = path.substr(std::string("/std/collections/vector/").size());
      if (allowsVectorStdlibCompatibilitySuffix(suffix)) {
        const std::string vectorAlias = "/vector/" + suffix;
        defIt = defMap.find(vectorAlias);
        if (defIt != defMap.end()) {
          return defIt->second;
        }
      }
      if (allowsArrayVectorCompatibilitySuffix(suffix)) {
        const std::string arrayAlias = "/array/" + suffix;
        defIt = defMap.find(arrayAlias);
        if (defIt != defMap.end()) {
          return defIt->second;
        }
      }
    }
    if (path.rfind("/map/", 0) == 0) {
      const std::string suffix = path.substr(std::string("/map/").size());
      if (isExplicitMapContainsOrTryAtCompatibilityMethodAlias &&
          (suffix == "contains" || suffix == "tryAt")) {
        return nullptr;
      }
      const std::string stdlibAlias = "/std/collections/map/" + path.substr(std::string("/map/").size());
      defIt = defMap.find(stdlibAlias);
      if (defIt != defMap.end()) {
        return defIt->second;
      }
    }
    if (path.rfind("/std/collections/map/", 0) == 0) {
      const std::string suffix = path.substr(std::string("/std/collections/map/").size());
      if (suffix != "map" && suffix != "count" && suffix != "contains" && suffix != "tryAt" &&
          suffix != "at" && suffix != "at_unsafe") {
        const std::string mapAlias = "/map/" + suffix;
        defIt = defMap.find(mapAlias);
        if (defIt != defMap.end()) {
          return defIt->second;
        }
      }
    }
    return nullptr;
  };
  if (!resolvedTypePath.empty()) {
    std::string normalizedResolvedTypePath = resolvedTypePath;
    if (!normalizedResolvedTypePath.empty() && normalizedResolvedTypePath.front() != '/') {
      normalizedResolvedTypePath.insert(normalizedResolvedTypePath.begin(), '/');
    }
    std::string resolvedTypeWithoutSlash = normalizedResolvedTypePath;
    if (!resolvedTypeWithoutSlash.empty() && resolvedTypeWithoutSlash.front() == '/') {
      resolvedTypeWithoutSlash.erase(resolvedTypeWithoutSlash.begin());
    }
    const std::string resolvedBase =
        (isExplicitMapContainsOrTryAtCompatibilityMethodAlias &&
         isMapReceiverTarget(resolvedTypeWithoutSlash))
            ? "/map"
            : (shouldPreferCanonicalMapPath(resolvedTypeWithoutSlash)
                   ? "/std/collections/map"
                   : normalizedResolvedTypePath);
    const std::string resolved = resolvedBase + "/" + normalizedMethodName;
    if (isExplicitRemovedVectorMethodAlias && isVectorReceiverTarget(resolvedTypeWithoutSlash)) {
      errorOut = "unknown method: " + resolved;
      return nullptr;
    }
    if (isExplicitMapMethodAlias && isMapReceiverTarget(resolvedTypeWithoutSlash)) {
      errorOut = "unknown method: " + resolved;
      return nullptr;
    }
    const Definition *resolvedDef = findMethodDefinitionByPath(resolved);
    if (resolvedDef == nullptr) {
      if (!isExplicitCanonicalMapMethodAlias && !isExplicitCompatibilityMapMethodAlias &&
          !isExplicitMapContainsOrTryAtCompatibilityMethodAlias &&
          isMapReceiverTarget(resolvedTypeWithoutSlash) &&
          (normalizedMethodName == "contains" || normalizedMethodName == "tryAt")) {
        errorOut.clear();
        return nullptr;
      }
      errorOut = "unknown method: " + resolved;
      return nullptr;
    }
    return resolvedDef;
  }

  if (normalizedTypeName.empty()) {
    errorOut = "unknown method target for " + normalizedMethodName;
    return nullptr;
  }

  const std::string resolvedBase =
      (isExplicitMapContainsOrTryAtCompatibilityMethodAlias &&
       isMapReceiverTarget(normalizedTypeName))
          ? "/map"
          : (shouldPreferCanonicalMapPath(normalizedTypeName)
                 ? "/std/collections/map"
                 : "/" + normalizedTypeName);
  const std::string resolved = resolvedBase + "/" + normalizedMethodName;
  if (isExplicitRemovedVectorMethodAlias && isVectorReceiverTarget(normalizedTypeName)) {
    errorOut = "unknown method: " + resolved;
    return nullptr;
  }
  if (isExplicitMapMethodAlias && isMapReceiverTarget(normalizedTypeName)) {
    errorOut = "unknown method: " + resolved;
    return nullptr;
  }
  const Definition *resolvedDef = findMethodDefinitionByPath(resolved);
  if (resolvedDef == nullptr) {
    if (!isExplicitCanonicalMapMethodAlias && !isExplicitCompatibilityMapMethodAlias &&
        !isExplicitMapContainsOrTryAtCompatibilityMethodAlias &&
        isMapReceiverTarget(normalizedTypeName) &&
        (normalizedMethodName == "contains" || normalizedMethodName == "tryAt")) {
      errorOut.clear();
      return nullptr;
    }
    errorOut = "unknown method: " + resolved;
    return nullptr;
  }
  return resolvedDef;
}

bool resolveReturnInfoKindForPath(const std::string &path,
                                  const GetReturnInfoForPathFn &getReturnInfo,
                                  bool requireArrayReturn,
                                  LocalInfo::ValueKind &kindOut) {
  kindOut = LocalInfo::ValueKind::Unknown;
  if (!getReturnInfo) {
    return false;
  }

  ReturnInfo info;
  if (!getReturnInfo(path, info) || info.returnsVoid) {
    return false;
  }

  if (requireArrayReturn) {
    if (!info.returnsArray) {
      return false;
    }
  } else if (info.returnsArray) {
    return false;
  }

  kindOut = info.kind;
  return true;
}

bool resolveMethodCallReturnKind(const Expr &methodCallExpr,
                                 const LocalMap &localsIn,
                                 const ResolveMethodCallDefinitionFn &resolveMethodCallDefinition,
                                 const ResolveDefinitionCallFn &resolveDefinitionCall,
                                 const GetReturnInfoForPathFn &getReturnInfo,
                                 bool requireArrayReturn,
                                 LocalInfo::ValueKind &kindOut,
                                 bool *methodResolvedOut) {
  kindOut = LocalInfo::ValueKind::Unknown;
  if (methodResolvedOut != nullptr) {
    *methodResolvedOut = false;
  }
  if (!resolveMethodCallDefinition) {
    return false;
  }

  const Definition *callee = resolveMethodCallDefinition(methodCallExpr, localsIn);
  if (callee == nullptr) {
    auto normalizeMethodName = [](std::string name) {
      if (!name.empty() && name.front() == '/') {
        name.erase(name.begin());
      }
      return name;
    };
    auto isBuiltinAccessMethodName = [](const std::string &name) {
      return name == "at" || name == "at_unsafe";
    };
    auto isBuiltinCountMethodName = [](const std::string &name) {
      return name == "count";
    };
    auto isBuiltinCapacityMethodName = [](const std::string &name) {
      return name == "capacity";
    };
    auto isBuiltinContainsMethodName = [](const std::string &name) {
      return name == "contains";
    };
    auto isBuiltinTryAtMethodName = [](const std::string &name) {
      return name == "tryAt";
    };
    auto isBareVectorAccessMethodExpr = [&](const Expr &expr) {
      if (!expr.isMethodCall || expr.args.size() != 2 ||
          !(isSimpleCallName(expr, "at") || isSimpleCallName(expr, "at_unsafe"))) {
        return false;
      }

      const Expr &receiverExpr = expr.args.front();
      if (receiverExpr.kind == Expr::Kind::Name) {
        auto localIt = localsIn.find(receiverExpr.name);
        if (localIt == localsIn.end()) {
          return false;
        }
        const LocalInfo &receiverInfo = localIt->second;
        return receiverInfo.kind == LocalInfo::Kind::Vector ||
               (receiverInfo.kind == LocalInfo::Kind::Reference && receiverInfo.referenceToVector) ||
               (receiverInfo.kind == LocalInfo::Kind::Pointer && receiverInfo.pointerToVector);
      }

      if (receiverExpr.kind == Expr::Kind::Call) {
        std::string collectionName;
        if (getBuiltinCollectionName(receiverExpr, collectionName)) {
          return collectionName == "vector" && receiverExpr.templateArgs.size() == 1;
        }

        const Definition *receiverDef = receiverExpr.isMethodCall
                                            ? resolveMethodCallDefinition(receiverExpr, localsIn)
                                            : (resolveDefinitionCall ? resolveDefinitionCall(receiverExpr) : nullptr);
        if (receiverDef == nullptr) {
          return false;
        }
        std::string receiverTypeName;
        return inferReceiverTypeFromDeclaredReturn(*receiverDef, receiverTypeName) &&
               receiverTypeName == "vector";
      }

      return false;
    };
    auto resolveBuiltinMethodReceiverKind = [&](const Expr &receiverExpr,
                                                const std::string &normalizedName,
                                                LocalInfo::ValueKind &builtinKindOut) {
      builtinKindOut = LocalInfo::ValueKind::Unknown;
      if (!methodCallExpr.isMethodCall || requireArrayReturn) {
        return false;
      }
      auto inferDeclaredReceiverCallKind = [&](LocalInfo::ValueKind &declaredKindOut) {
        declaredKindOut = LocalInfo::ValueKind::Unknown;
        if (receiverExpr.kind != Expr::Kind::Call) {
          return false;
        }
        const Definition *receiverDef = receiverExpr.isMethodCall
                                            ? resolveMethodCallDefinition(receiverExpr, localsIn)
                                            : (resolveDefinitionCall ? resolveDefinitionCall(receiverExpr) : nullptr);
        if (receiverDef == nullptr) {
          return false;
        }
        std::string collectionName;
        std::vector<std::string> collectionArgs;
        if (!inferDeclaredReturnCollection(*receiverDef, collectionName, collectionArgs)) {
          return false;
        }
        if ((collectionName == "array" || collectionName == "vector" || collectionName == "soa_vector") &&
            collectionArgs.size() == 1) {
          declaredKindOut = valueKindFromTypeName(collectionArgs.front());
          return declaredKindOut != LocalInfo::ValueKind::Unknown;
        }
        if (collectionName == "map" && collectionArgs.size() == 2) {
          declaredKindOut = valueKindFromTypeName(collectionArgs.back());
          return declaredKindOut != LocalInfo::ValueKind::Unknown;
        }
        return false;
      };

      if (isBuiltinAccessMethodName(normalizedName)) {
        const auto arrayVectorTargetInfo = resolveArrayVectorAccessTargetInfo(receiverExpr, localsIn);
        if (arrayVectorTargetInfo.isArrayOrVectorTarget &&
            arrayVectorTargetInfo.elemKind != LocalInfo::ValueKind::Unknown) {
          builtinKindOut = arrayVectorTargetInfo.elemKind;
          return true;
        }
        const auto mapTargetInfo = resolveMapAccessTargetInfo(receiverExpr, localsIn);
        if (mapTargetInfo.isMapTarget && mapTargetInfo.mapValueKind != LocalInfo::ValueKind::Unknown) {
          builtinKindOut = mapTargetInfo.mapValueKind;
          return true;
        }
        return inferDeclaredReceiverCallKind(builtinKindOut);
      }

      if (isBuiltinCountMethodName(normalizedName) || isBuiltinCapacityMethodName(normalizedName)) {
        const auto arrayVectorTargetInfo = resolveArrayVectorAccessTargetInfo(receiverExpr, localsIn);
        if (arrayVectorTargetInfo.isArrayOrVectorTarget) {
          builtinKindOut = LocalInfo::ValueKind::Int32;
          return true;
        }
        const auto mapTargetInfo = resolveMapAccessTargetInfo(receiverExpr, localsIn);
        if (mapTargetInfo.isMapTarget) {
          builtinKindOut = LocalInfo::ValueKind::Int32;
          return true;
        }
        LocalInfo::ValueKind declaredReceiverKind = LocalInfo::ValueKind::Unknown;
        if (inferDeclaredReceiverCallKind(declaredReceiverKind)) {
          builtinKindOut = LocalInfo::ValueKind::Int32;
          return true;
        }
      }

      if (isBuiltinContainsMethodName(normalizedName)) {
        const auto mapTargetInfo = resolveMapAccessTargetInfo(receiverExpr, localsIn);
        if (mapTargetInfo.isMapTarget) {
          builtinKindOut = LocalInfo::ValueKind::Bool;
          return true;
        }
        LocalInfo::ValueKind declaredReceiverKind = LocalInfo::ValueKind::Unknown;
        if (inferDeclaredReceiverCallKind(declaredReceiverKind)) {
          builtinKindOut = LocalInfo::ValueKind::Bool;
          return true;
        }
      }

      if (isBuiltinTryAtMethodName(normalizedName)) {
        const auto mapTargetInfo = resolveMapAccessTargetInfo(receiverExpr, localsIn);
        if (mapTargetInfo.isMapTarget && mapTargetInfo.mapValueKind != LocalInfo::ValueKind::Unknown) {
          builtinKindOut = mapTargetInfo.mapValueKind;
          return true;
        }
        return inferDeclaredReceiverCallKind(builtinKindOut);
      }

      return false;
    };

    auto resolveExplicitRemovedBuiltinAccessMethodReturnKind = [&](LocalInfo::ValueKind &builtinKindOut) {
      builtinKindOut = LocalInfo::ValueKind::Unknown;
      if (!methodCallExpr.isMethodCall || requireArrayReturn || methodCallExpr.args.size() != 2) {
        return false;
      }

      std::string normalizedName = methodCallExpr.name;
      if (!normalizedName.empty() && normalizedName.front() == '/') {
        normalizedName.erase(normalizedName.begin());
      }
      if (normalizedName == "vector/at" || normalizedName == "vector/at_unsafe" ||
          normalizedName == "std/collections/vector/at" ||
          normalizedName == "std/collections/vector/at_unsafe" ||
          normalizedName == "map/at" || normalizedName == "map/at_unsafe" ||
          normalizedName == "std/collections/map/at" ||
          normalizedName == "std/collections/map/at_unsafe") {
        return false;
      }
      const Expr &receiverExpr = methodCallExpr.args.front();
      auto assignKnownElementKind = [&](LocalInfo::ValueKind valueKind) {
        if (valueKind == LocalInfo::ValueKind::Unknown) {
          return false;
        }
        builtinKindOut = valueKind;
        return true;
      };

      auto assignMapValueKind = [&](const LocalInfo &receiverInfo) {
        if (receiverInfo.kind == LocalInfo::Kind::Map ||
            (receiverInfo.kind == LocalInfo::Kind::Reference && receiverInfo.referenceToMap) ||
            (receiverInfo.kind == LocalInfo::Kind::Pointer && receiverInfo.pointerToMap)) {
          return assignKnownElementKind(receiverInfo.mapValueKind);
        }
        return false;
      };
      auto assignDeclaredReceiverCallKind = [&](const Definition &receiverDef) {
        std::string collectionName;
        std::vector<std::string> collectionArgs;
        if (inferDeclaredReturnCollection(receiverDef, collectionName, collectionArgs)) {
          if ((normalizedName == "vector/at" || normalizedName == "vector/at_unsafe" ||
               normalizedName == "std/collections/vector/at" ||
               normalizedName == "std/collections/vector/at_unsafe") &&
              (collectionName == "vector" || collectionName == "array") && collectionArgs.size() == 1) {
            return assignKnownElementKind(valueKindFromTypeName(collectionArgs.front()));
          }
          if ((normalizedName == "map/at" || normalizedName == "map/at_unsafe" ||
               normalizedName == "std/collections/map/at" ||
               normalizedName == "std/collections/map/at_unsafe") &&
              collectionName == "map" && collectionArgs.size() == 2) {
            return assignKnownElementKind(valueKindFromTypeName(collectionArgs[1]));
          }
        }

        ReturnInfo receiverInfo;
        if (getReturnInfo && getReturnInfo(receiverDef.fullPath, receiverInfo) && !receiverInfo.returnsVoid &&
            !receiverInfo.returnsArray &&
            (normalizedName == "vector/at" || normalizedName == "vector/at_unsafe" ||
             normalizedName == "std/collections/vector/at" ||
             normalizedName == "std/collections/vector/at_unsafe") &&
            receiverInfo.kind == LocalInfo::ValueKind::String) {
          return assignKnownElementKind(LocalInfo::ValueKind::Int32);
        }
        return false;
      };

      if (receiverExpr.kind == Expr::Kind::Name) {
        auto localIt = localsIn.find(receiverExpr.name);
        if (localIt == localsIn.end()) {
          return false;
        }
        const LocalInfo &receiverInfo = localIt->second;
        if (normalizedName == "vector/at" || normalizedName == "vector/at_unsafe" ||
            normalizedName == "std/collections/vector/at" ||
            normalizedName == "std/collections/vector/at_unsafe") {
          if (receiverInfo.kind == LocalInfo::Kind::Vector || receiverInfo.kind == LocalInfo::Kind::Array ||
              (receiverInfo.kind == LocalInfo::Kind::Reference &&
               (receiverInfo.referenceToArray || receiverInfo.referenceToVector))) {
            return assignKnownElementKind(receiverInfo.valueKind);
          }
          return false;
        }
        if (normalizedName == "map/at" || normalizedName == "map/at_unsafe" ||
            normalizedName == "std/collections/map/at" ||
            normalizedName == "std/collections/map/at_unsafe") {
          return assignMapValueKind(receiverInfo);
        }
        return false;
      }

      if (receiverExpr.kind != Expr::Kind::Call) {
        return false;
      }
      std::string collectionName;
      if (!getBuiltinCollectionName(receiverExpr, collectionName)) {
        return false;
      }
      if ((normalizedName == "vector/at" || normalizedName == "vector/at_unsafe" ||
           normalizedName == "std/collections/vector/at" ||
           normalizedName == "std/collections/vector/at_unsafe") &&
          (collectionName == "vector" || collectionName == "array") && receiverExpr.templateArgs.size() == 1) {
        return assignKnownElementKind(valueKindFromTypeName(receiverExpr.templateArgs.front()));
      }
      if ((normalizedName == "map/at" || normalizedName == "map/at_unsafe" ||
           normalizedName == "std/collections/map/at" ||
           normalizedName == "std/collections/map/at_unsafe") &&
          collectionName == "map" && receiverExpr.templateArgs.size() == 2) {
        return assignKnownElementKind(valueKindFromTypeName(receiverExpr.templateArgs[1]));
      }
      if (const Definition *receiverDef = resolveMethodCallDefinition(receiverExpr, localsIn)) {
        return assignDeclaredReceiverCallKind(*receiverDef);
      }
      return false;
    };

    if (resolveExplicitRemovedBuiltinAccessMethodReturnKind(kindOut)) {
      if (methodResolvedOut != nullptr) {
        *methodResolvedOut = true;
      }
      return true;
    }
    if (!isBareVectorAccessMethodExpr(methodCallExpr) && !methodCallExpr.args.empty() &&
        resolveBuiltinMethodReceiverKind(
            methodCallExpr.args.front(), normalizeMethodName(methodCallExpr.name), kindOut)) {
      if (methodResolvedOut != nullptr) {
        *methodResolvedOut = true;
      }
      return true;
    }
    return false;
  }

  if (methodResolvedOut != nullptr) {
    *methodResolvedOut = true;
  }
  return resolveReturnInfoKindForPath(callee->fullPath, getReturnInfo, requireArrayReturn, kindOut);
}

bool resolveDefinitionCallReturnKind(const Expr &callExpr,
                                     const std::unordered_map<std::string, const Definition *> &defMap,
                                     const ResolveReceiverExprPathFn &resolveExprPath,
                                     const GetReturnInfoForPathFn &getReturnInfo,
                                     bool requireArrayReturn,
                                     LocalInfo::ValueKind &kindOut,
                                     bool *definitionMatchedOut) {
  kindOut = LocalInfo::ValueKind::Unknown;
  if (definitionMatchedOut != nullptr) {
    *definitionMatchedOut = false;
  }

  if (callExpr.kind != Expr::Kind::Call || callExpr.isMethodCall) {
    return false;
  }

  auto candidates = collectionHelperPathCandidates(resolveExprPath(callExpr));
  pruneRemovedVectorCompatibilityCallReturnCandidates(candidates, resolveExprPath(callExpr));
  pruneRemovedMapCompatibilityCallReturnCandidates(candidates, resolveExprPath(callExpr));
  bool matchedDefinition = false;
  for (const auto &candidate : candidates) {
    auto defIt = defMap.find(candidate);
    if (defIt == defMap.end()) {
      continue;
    }
    matchedDefinition = true;
    if (resolveReturnInfoKindForPath(candidate, getReturnInfo, requireArrayReturn, kindOut)) {
      if (definitionMatchedOut != nullptr) {
        *definitionMatchedOut = true;
      }
      return true;
    }
  }

  if (definitionMatchedOut != nullptr) {
    *definitionMatchedOut = matchedDefinition;
  }
  return false;
}

bool resolveCountMethodCallReturnKind(const Expr &callExpr,
                                      const LocalMap &localsIn,
                                      const IsMethodCallClassifierFn &isArrayCountCall,
                                      const IsMethodCallClassifierFn &isStringCountCall,
                                      const ResolveMethodCallDefinitionFn &resolveMethodCallDefinition,
                                      const GetReturnInfoForPathFn &getReturnInfo,
                                      bool requireArrayReturn,
                                      LocalInfo::ValueKind &kindOut,
                                      bool *methodResolvedOut,
                                      const InferReceiverExprKindFn &inferExprKind) {
  kindOut = LocalInfo::ValueKind::Unknown;
  if (methodResolvedOut != nullptr) {
    *methodResolvedOut = false;
  }

  if (callExpr.kind != Expr::Kind::Call || callExpr.isMethodCall) {
    return false;
  }
  if (isExplicitRemovedVectorMethodAliasPath(callExpr.name)) {
    return false;
  }
  if (isExplicitMapHelperFallbackPath(callExpr)) {
    return false;
  }
  const bool isCountCall = (isVectorBuiltinName(callExpr, "count") || isMapBuiltinName(callExpr, "count")) &&
                           callExpr.args.size() == 1;
  const bool isContainsCall = isSimpleCallName(callExpr, "contains") && callExpr.args.size() == 2;
  std::string accessName;
  const bool isCollectionAccessCall = getBuiltinArrayAccessName(callExpr, accessName);
  const bool isAccessCall = (isCollectionAccessCall || isSimpleCallName(callExpr, "get") ||
                             isSimpleCallName(callExpr, "ref")) &&
                            callExpr.args.size() == 2;
  const bool isVectorMutatorCall =
      isVectorBuiltinName(callExpr, "push") || isVectorBuiltinName(callExpr, "pop") ||
      isVectorBuiltinName(callExpr, "reserve") || isVectorBuiltinName(callExpr, "clear") ||
      isVectorBuiltinName(callExpr, "remove_at") || isVectorBuiltinName(callExpr, "remove_swap");
  auto expectedVectorMutatorArgCount = [&]() -> size_t {
    if (isVectorBuiltinName(callExpr, "pop") || isVectorBuiltinName(callExpr, "clear")) {
      return 1u;
    }
    return 2u;
  };
  size_t expectedArgCount = 1u;
  if (isAccessCall || isContainsCall) {
    expectedArgCount = 2u;
  } else if (isVectorMutatorCall) {
    expectedArgCount = expectedVectorMutatorArgCount();
  }
  const bool isSoaFieldHelperCall =
      callExpr.args.size() == 1 && !isCountCall && isSoaVectorReceiverExpr(callExpr.args.front(), localsIn);
  if ((!isCountCall && !isContainsCall && !isAccessCall && !isSoaFieldHelperCall && !isVectorMutatorCall) ||
      callExpr.args.size() != expectedArgCount) {
    return false;
  }
  (void)isArrayCountCall;
  (void)isStringCountCall;

  auto hasNamedArgs = [&]() {
    for (const auto &argName : callExpr.argNames) {
      if (argName.has_value()) {
        return true;
      }
    }
    return false;
  };
  auto isKnownCollectionAccessReceiverExpr = [&](const Expr &candidate) -> bool {
    if (candidate.kind == Expr::Kind::StringLiteral) {
      return true;
    }
    if (resolveMapAccessTargetInfo(candidate, localsIn).isMapTarget) {
      return true;
    }
    if (resolveArrayVectorAccessTargetInfo(candidate, localsIn).isArrayOrVectorTarget) {
      return true;
    }
    if (candidate.kind != Expr::Kind::Name) {
      return false;
    }
    auto it = localsIn.find(candidate.name);
    return it != localsIn.end() && it->second.kind == LocalInfo::Kind::Value &&
           it->second.valueKind == LocalInfo::ValueKind::String;
  };
  auto isKnownMapReceiverExpr = [&](const Expr &candidate) -> bool {
    return resolveMapAccessTargetInfo(candidate, localsIn).isMapTarget;
  };
  auto isKnownVectorMutatorReceiverExpr = [&](const Expr &candidate) -> bool {
    if (candidate.kind != Expr::Kind::Name) {
      return false;
    }
    auto it = localsIn.find(candidate.name);
    if (it == localsIn.end()) {
      return false;
    }
    const LocalInfo &info = it->second;
    return info.kind == LocalInfo::Kind::Vector || info.isSoaVector ||
           (info.kind == LocalInfo::Kind::Pointer && info.pointerToVector);
  };
  auto isKnownLocalName = [&](const Expr &candidate) -> bool {
    if (candidate.kind != Expr::Kind::Name) {
      return false;
    }
    return localsIn.find(candidate.name) != localsIn.end();
  };
  auto isStringAccessReceiverExpr = [&](const Expr &candidate) {
    return !requireArrayReturn && inferExprKind &&
           inferExprKind(candidate, localsIn) == LocalInfo::ValueKind::String;
  };
  auto buildMethodExprForReceiverIndex = [&](size_t receiverIndex) {
    Expr methodExpr = callExpr;
    methodExpr.isMethodCall = true;
    std::string normalizedHelperName;
    if (resolveVectorHelperAliasName(methodExpr, normalizedHelperName)) {
      methodExpr.name = normalizedHelperName;
    } else if (resolveMapHelperAliasName(methodExpr, normalizedHelperName)) {
      methodExpr.name = normalizedHelperName;
    }
    if (receiverIndex != 0 && receiverIndex < methodExpr.args.size()) {
      std::swap(methodExpr.args[0], methodExpr.args[receiverIndex]);
      if (methodExpr.argNames.size() < methodExpr.args.size()) {
        methodExpr.argNames.resize(methodExpr.args.size());
      }
      std::swap(methodExpr.argNames[0], methodExpr.argNames[receiverIndex]);
    }
    return methodExpr;
  };

  std::vector<size_t> receiverIndices;
  auto appendReceiverIndex = [&](size_t index) {
    if (index >= callExpr.args.size()) {
      return;
    }
    for (size_t existing : receiverIndices) {
      if (existing == index) {
        return;
      }
    }
    receiverIndices.push_back(index);
  };
  const bool hasNamedArgsValue = hasNamedArgs();
  if (hasNamedArgsValue) {
    bool hasValuesNamedReceiver = false;
    if (isVectorMutatorCall || isAccessCall || isContainsCall) {
      for (size_t i = 0; i < callExpr.args.size(); ++i) {
        if (i < callExpr.argNames.size() && callExpr.argNames[i].has_value() &&
            *callExpr.argNames[i] == "values") {
          appendReceiverIndex(i);
          hasValuesNamedReceiver = true;
        }
      }
    }
    if (!hasValuesNamedReceiver) {
      appendReceiverIndex(0);
      for (size_t i = 1; i < callExpr.args.size(); ++i) {
        appendReceiverIndex(i);
      }
    }
  } else {
    appendReceiverIndex(0);
  }
  const bool probePositionalReorderedVectorMutatorReceiver =
      isVectorMutatorCall && !hasNamedArgsValue && callExpr.args.size() > 1 &&
      (callExpr.args.front().kind == Expr::Kind::Literal ||
       callExpr.args.front().kind == Expr::Kind::BoolLiteral ||
       callExpr.args.front().kind == Expr::Kind::FloatLiteral ||
       callExpr.args.front().kind == Expr::Kind::StringLiteral ||
       (callExpr.args.front().kind == Expr::Kind::Name &&
        isKnownLocalName(callExpr.args.front()) &&
        !isKnownVectorMutatorReceiverExpr(callExpr.args.front())));
  if (probePositionalReorderedVectorMutatorReceiver) {
    for (size_t i = 1; i < callExpr.args.size(); ++i) {
      appendReceiverIndex(i);
    }
  }
  const bool probePositionalReorderedAccessReceiver =
      (isAccessCall || isContainsCall) && !hasNamedArgsValue && callExpr.args.size() > 1 &&
      (callExpr.args.front().kind == Expr::Kind::Literal || callExpr.args.front().kind == Expr::Kind::BoolLiteral ||
       callExpr.args.front().kind == Expr::Kind::FloatLiteral || callExpr.args.front().kind == Expr::Kind::StringLiteral ||
       (callExpr.args.front().kind == Expr::Kind::Name &&
        isKnownLocalName(callExpr.args.front()) &&
        !isKnownCollectionAccessReceiverExpr(callExpr.args.front())));
  if (probePositionalReorderedAccessReceiver) {
    for (size_t i = 1; i < callExpr.args.size(); ++i) {
      appendReceiverIndex(i);
    }
  }
  const bool hasAlternativeCollectionReceiver =
      probePositionalReorderedAccessReceiver &&
      std::any_of(receiverIndices.begin(), receiverIndices.end(), [&](size_t index) {
        return index > 0 && index < callExpr.args.size() &&
               isKnownCollectionAccessReceiverExpr(callExpr.args[index]);
      });

  for (size_t receiverIndex : receiverIndices) {
    if (hasAlternativeCollectionReceiver && receiverIndex == 0) {
      continue;
    }
    Expr methodExpr = buildMethodExprForReceiverIndex(receiverIndex);
    if (isContainsCall && isKnownMapReceiverExpr(methodExpr.args.front())) {
      if (methodResolvedOut != nullptr) {
        *methodResolvedOut = true;
      }
      kindOut = LocalInfo::ValueKind::Bool;
      return true;
    }
    if (!resolveMethodCallDefinition) {
      continue;
    }
    const Definition *callee = resolveMethodCallDefinition(methodExpr, localsIn);
    if (callee == nullptr || !isAllowedResolvedVectorDirectCallPath(callExpr.name, callee->fullPath) ||
        !isAllowedResolvedMapDirectCallPath(callExpr.name, callee->fullPath)) {
      continue;
    }
    if (resolveReturnInfoKindForPath(callee->fullPath, getReturnInfo, requireArrayReturn, kindOut)) {
      if (methodResolvedOut != nullptr) {
        *methodResolvedOut = true;
      }
      return true;
    }
    if (methodResolvedOut != nullptr) {
      *methodResolvedOut = true;
    }
    if (isAccessCall && isStringAccessReceiverExpr(methodExpr.args.front())) {
      if (methodResolvedOut != nullptr) {
        *methodResolvedOut = true;
      }
      kindOut = LocalInfo::ValueKind::Int32;
      return true;
    }
    if (isCountCall && !requireArrayReturn && inferExprKind &&
        inferExprKind(methodExpr.args.front(), localsIn) == LocalInfo::ValueKind::String) {
      if (methodResolvedOut != nullptr) {
        *methodResolvedOut = true;
      }
      kindOut = LocalInfo::ValueKind::Int32;
      return true;
    }
    return false;
  }

  if (isCountCall && !requireArrayReturn && inferExprKind &&
      inferExprKind(callExpr.args.front(), localsIn) == LocalInfo::ValueKind::String) {
    kindOut = LocalInfo::ValueKind::Int32;
    return true;
  }
  if (isAccessCall && isStringAccessReceiverExpr(callExpr.args.front())) {
    kindOut = LocalInfo::ValueKind::Int32;
    return true;
  }
  if (isContainsCall && isKnownMapReceiverExpr(callExpr.args.front())) {
    kindOut = LocalInfo::ValueKind::Bool;
    return true;
  }

  return false;
}

bool resolveCapacityMethodCallReturnKind(const Expr &callExpr,
                                         const LocalMap &localsIn,
                                         const ResolveMethodCallDefinitionFn &resolveMethodCallDefinition,
                                         const GetReturnInfoForPathFn &getReturnInfo,
                                         bool requireArrayReturn,
                                         LocalInfo::ValueKind &kindOut,
                                         bool *methodResolvedOut) {
  kindOut = LocalInfo::ValueKind::Unknown;
  if (methodResolvedOut != nullptr) {
    *methodResolvedOut = false;
  }

  if (callExpr.kind != Expr::Kind::Call || callExpr.isMethodCall) {
    return false;
  }
  if (isExplicitRemovedVectorMethodAliasPath(callExpr.name)) {
    return false;
  }
  if (!isVectorBuiltinName(callExpr, "capacity") || callExpr.args.size() != 1) {
    return false;
  }

  Expr methodExpr = callExpr;
  methodExpr.isMethodCall = true;
  std::string normalizedHelperName;
  if (resolveVectorHelperAliasName(methodExpr, normalizedHelperName)) {
    methodExpr.name = normalizedHelperName;
  }
  return resolveMethodCallReturnKind(methodExpr,
                                     localsIn,
                                     resolveMethodCallDefinition,
                                     {},
                                     getReturnInfo,
                                     requireArrayReturn,
                                     kindOut,
                                     methodResolvedOut);
}

const Definition *resolveMethodCallDefinitionFromExpr(
    const Expr &callExpr,
    const LocalMap &localsIn,
    const IsMethodCallClassifierFn &isArrayCountCall,
    const IsMethodCallClassifierFn &isVectorCapacityCall,
    const IsMethodCallClassifierFn &isEntryArgsName,
    const std::unordered_map<std::string, std::string> &importAliases,
    const std::unordered_set<std::string> &structNames,
    const InferReceiverExprKindFn &inferExprKind,
    const ResolveReceiverExprPathFn &resolveExprPath,
    const std::unordered_map<std::string, const Definition *> &defMap,
    std::string &errorOut) {
  return resolveMethodCallDefinitionFromExpr(callExpr,
                                             localsIn,
                                             isArrayCountCall,
                                             isVectorCapacityCall,
                                             isEntryArgsName,
                                             importAliases,
                                             structNames,
                                             inferExprKind,
                                             resolveExprPath,
                                             {},
                                             defMap,
                                             errorOut);
}

const Definition *resolveMethodCallDefinitionFromExpr(
    const Expr &callExpr,
    const LocalMap &localsIn,
    const IsMethodCallClassifierFn &isArrayCountCall,
    const IsMethodCallClassifierFn &isVectorCapacityCall,
    const IsMethodCallClassifierFn &isEntryArgsName,
    const std::unordered_map<std::string, std::string> &importAliases,
    const std::unordered_set<std::string> &structNames,
    const InferReceiverExprKindFn &inferExprKind,
    const ResolveReceiverExprPathFn &resolveExprPath,
    const GetReturnInfoForPathFn &getReturnInfo,
    const std::unordered_map<std::string, const Definition *> &defMap,
    std::string &errorOut) {
  if (callExpr.kind != Expr::Kind::Call || callExpr.isBinding) {
    return nullptr;
  }
  if (!callExpr.isMethodCall) {
    const std::string resolvedPath = resolveExprPath(callExpr);
    auto defIt = defMap.find(resolvedPath);
    if (defIt != defMap.end()) {
      return defIt->second;
    }
    return nullptr;
  }

  std::string accessName;
  const bool isBuiltinAccessCall = getBuiltinArrayAccessName(callExpr, accessName) && callExpr.args.size() == 2;
  const bool isBuiltinCountOrCapacityCall =
      isVectorBuiltinName(callExpr, "count") || isMapBuiltinName(callExpr, "count") ||
      isVectorBuiltinName(callExpr, "capacity");
  const bool isBuiltinBareVectorCapacityMethod =
      isSimpleCallName(callExpr, "capacity") &&
      isVectorCapacityCall && isVectorCapacityCall(callExpr, localsIn);
  const bool isBuiltinBareVectorAccessMethod =
      callExpr.isMethodCall && callExpr.args.size() == 2 &&
      (isSimpleCallName(callExpr, "at") || isSimpleCallName(callExpr, "at_unsafe")) &&
      resolveArrayVectorAccessTargetInfo(callExpr.args.front(), localsIn).isVectorTarget;
  const bool isBuiltinBareVectorMutatorMethod =
      callExpr.isMethodCall &&
      (isSimpleCallName(callExpr, "push") || isSimpleCallName(callExpr, "pop") ||
       isSimpleCallName(callExpr, "reserve") || isSimpleCallName(callExpr, "clear") ||
       isSimpleCallName(callExpr, "remove_at") || isSimpleCallName(callExpr, "remove_swap")) &&
      !callExpr.args.empty() && resolveArrayVectorAccessTargetInfo(callExpr.args.front(), localsIn).isVectorTarget;
  const bool isBuiltinVectorMutatorCall =
      isVectorBuiltinName(callExpr, "push") || isVectorBuiltinName(callExpr, "pop") ||
      isVectorBuiltinName(callExpr, "reserve") || isVectorBuiltinName(callExpr, "clear") ||
      isVectorBuiltinName(callExpr, "remove_at") || isVectorBuiltinName(callExpr, "remove_swap");
  const bool isExplicitRemovedVectorMethodAlias = isExplicitRemovedVectorMethodAliasPath(callExpr.name);
  const bool isExplicitMapMethodAlias = isExplicitMapMethodAliasPath(callExpr.name);
  const bool isBuiltinMapContainsOrTryAtCall =
      isSimpleCallName(callExpr, "contains") || isSimpleCallName(callExpr, "tryAt");
  const bool allowBuiltinFallback =
      !isExplicitRemovedVectorMethodAlias && !isExplicitMapMethodAlias &&
      !isBuiltinBareVectorCapacityMethod && !isBuiltinBareVectorAccessMethod &&
      !isBuiltinBareVectorMutatorMethod &&
      (isBuiltinCountOrCapacityCall || isBuiltinVectorMutatorCall ||
       isBuiltinMapContainsOrTryAtCall ||
       (isArrayCountCall && isArrayCountCall(callExpr, localsIn)) ||
       (isVectorCapacityCall && isVectorCapacityCall(callExpr, localsIn)) || isBuiltinAccessCall);

  const std::string priorError = errorOut;
  const Expr *receiver = nullptr;
  if (!resolveMethodCallReceiverExpr(callExpr,
                                     localsIn,
                                     isArrayCountCall,
                                     isVectorCapacityCall,
                                     isEntryArgsName,
                                     receiver,
                                     errorOut)) {
    if (allowBuiltinFallback) {
      errorOut = priorError;
    }
    return nullptr;
  }
  if (receiver == nullptr) {
    return nullptr;
  }

  std::string typeName;
  std::string resolvedTypePath;
  if (!resolveMethodReceiverTarget(*receiver,
                                   localsIn,
                                   callExpr.name,
                                   importAliases,
                                   structNames,
                                   inferExprKind,
                                   resolveExprPath,
                                   typeName,
                                   resolvedTypePath,
                                   errorOut)) {
    if (allowBuiltinFallback) {
      errorOut = priorError;
    }
    return nullptr;
  }
  std::string lookupError;
  const Definition *resolvedDef = resolveMethodDefinitionFromReceiverTarget(
      callExpr.name, typeName, resolvedTypePath, defMap, lookupError);
  auto resolveMethodDefinitionFromTypeNameWithAliasFallback = [&](const std::string &receiverTypeName,
                                                                  std::string &errorOutRef) -> const Definition * {
    if (receiverTypeName.empty()) {
      return nullptr;
    }
    const Definition *resolved = resolveMethodDefinitionFromReceiverTarget(
        callExpr.name, receiverTypeName, "", defMap, errorOutRef);
    if (resolved != nullptr) {
      return resolved;
    }
    if (receiverTypeName == "vector") {
      resolved = resolveMethodDefinitionFromReceiverTarget(
          callExpr.name, "", "/std/collections/vector", defMap, errorOutRef);
      if (resolved != nullptr) {
        return resolved;
      }
    }
    auto aliasIt = importAliases.find(receiverTypeName);
    if (aliasIt == importAliases.end()) {
      return nullptr;
    }
    const std::string aliasTypeName = normalizeMapImportAliasPath(aliasIt->second);
    if (aliasTypeName.empty()) {
      return nullptr;
    }
    return resolveMethodDefinitionFromReceiverTarget(callExpr.name, aliasTypeName, "", defMap, errorOutRef);
  };
  auto resolveStructTypePathFromScope = [&](const std::string &typeName,
                                            const std::string &namespacePrefix) -> std::string {
    if (typeName.empty()) {
      return "";
    }
    if (typeName.front() == '/') {
      return structNames.count(typeName) > 0 ? typeName : "";
    }
    std::string current = namespacePrefix;
    while (true) {
      if (!current.empty()) {
        const std::string scoped = current + "/" + typeName;
        if (structNames.count(scoped) > 0) {
          return scoped;
        }
      } else {
        const std::string root = "/" + typeName;
        if (structNames.count(root) > 0) {
          return root;
        }
      }
      if (current.empty()) {
        break;
      }
      const size_t slash = current.find_last_of('/');
      if (slash == std::string::npos || slash == 0) {
        current.clear();
      } else {
        current.erase(slash);
      }
    }
    auto importIt = importAliases.find(typeName);
    if (importIt != importAliases.end() && structNames.count(importIt->second) > 0) {
      return importIt->second;
    }
    return "";
  };
  auto inferStructReturnPathFromReceiverDef = [&](const Definition &definition) -> std::string {
    std::function<std::string(const Expr &, std::unordered_set<std::string> &)> inferStructExprPathForCall;
    inferStructExprPathForCall = [&](const Expr &expr, std::unordered_set<std::string> &visitedDefs) -> std::string {
      if (expr.kind == Expr::Kind::Name) {
        return resolveStructTypePathFromScope(expr.name, expr.namespacePrefix);
      }
      if (expr.kind != Expr::Kind::Call) {
        return "";
      }
      const std::string resolvedPath = resolveExprPath(expr);
      if (structNames.count(resolvedPath) > 0) {
        return resolvedPath;
      }
      auto defIt = defMap.find(resolvedPath);
      if (defIt != defMap.end() && defIt->second != nullptr && visitedDefs.insert(resolvedPath).second) {
        const Definition &nestedDef = *defIt->second;
        std::string inferred = inferStructReturnPathFromDefinition(
            nestedDef,
            [&](const std::string &typeName, const std::string &namespacePrefix, std::string &resolvedOut) {
              resolvedOut = resolveStructTypePathFromScope(typeName, namespacePrefix);
              return !resolvedOut.empty();
            },
            [&](const Expr &nestedExpr) { return inferStructExprPathForCall(nestedExpr, visitedDefs); });
        visitedDefs.erase(resolvedPath);
        if (!inferred.empty()) {
          return inferred;
        }
      }
      return resolveMethodReceiverStructTypePathFromCallExpr(expr, resolvedPath, importAliases, structNames);
    };
    std::unordered_set<std::string> visitedDefs = {definition.fullPath};
    return inferStructReturnPathFromDefinition(
        definition,
        [&](const std::string &typeName, const std::string &namespacePrefix, std::string &resolvedOut) {
          resolvedOut = resolveStructTypePathFromScope(typeName, namespacePrefix);
          return !resolvedOut.empty();
        },
        [&](const Expr &expr) { return inferStructExprPathForCall(expr, visitedDefs); });
  };
  if (resolvedDef == nullptr && typeName.empty() && resolvedTypePath.empty() &&
      receiver->kind == Expr::Kind::Call) {
    std::string nestedError = lookupError;
    const Definition *receiverDef = nullptr;
    std::string receiverPath = resolveExprPath(*receiver);
    if (receiverPath.empty() && !receiver->name.empty()) {
      receiverPath = receiver->name;
      if (!receiverPath.empty() && receiverPath.front() != '/') {
        receiverPath.insert(receiverPath.begin(), '/');
      }
    }
    if (!receiverPath.empty()) {
      auto receiverDefIt = defMap.find(receiverPath);
      if (receiverDefIt != defMap.end()) {
        receiverDef = receiverDefIt->second;
      }
    }
    if (receiverDef == nullptr && receiver->isMethodCall) {
      receiverDef = resolveMethodCallDefinitionFromExpr(*receiver,
                                                        localsIn,
                                                        isArrayCountCall,
                                                        isVectorCapacityCall,
                                                        isEntryArgsName,
                                                        importAliases,
                                                        structNames,
                                                        inferExprKind,
                                                        resolveExprPath,
                                                        getReturnInfo,
                                                        defMap,
                                                        nestedError);
    }
    if (receiverDef != nullptr) {
      resolvedTypePath = inferStructReturnPathFromReceiverDef(*receiverDef);
      if (!resolvedTypePath.empty()) {
        lookupError.clear();
        resolvedDef = resolveMethodDefinitionFromReceiverTarget(callExpr.name, "", resolvedTypePath, defMap, lookupError);
      }
    }
    if (resolvedDef == nullptr && receiverDef != nullptr && inferReceiverTypeFromDeclaredReturn(*receiverDef, typeName)) {
      lookupError.clear();
      resolvedDef = resolveMethodDefinitionFromTypeNameWithAliasFallback(typeName, lookupError);
    } else if (resolvedDef == nullptr && receiverDef != nullptr) {
      LocalInfo::ValueKind receiverKind = LocalInfo::ValueKind::Unknown;
      if (resolveReturnInfoKindForPath(receiverDef->fullPath, getReturnInfo, false, receiverKind)) {
        typeName = typeNameForValueKind(receiverKind);
        if (!typeName.empty()) {
          lookupError.clear();
          resolvedDef = resolveMethodDefinitionFromTypeNameWithAliasFallback(typeName, lookupError);
        }
      }
    } else {
      LocalInfo::ValueKind inferredReceiverKind = LocalInfo::ValueKind::Unknown;
      const bool blocksExplicitVectorAccessKindFallback = isExplicitVectorAccessHelperExpr(*receiver);
      if (!inferBuiltinAccessReceiverResultKind(
              *receiver, localsIn, inferExprKind, resolveExprPath, getReturnInfo, defMap, inferredReceiverKind) &&
          inferExprKind && !blocksExplicitVectorAccessKindFallback) {
        inferredReceiverKind = inferExprKind(*receiver, localsIn);
      }
      const std::string inferredReceiverTypeName = typeNameForValueKind(inferredReceiverKind);
      if (!inferredReceiverTypeName.empty()) {
        lookupError.clear();
        resolvedDef = resolveMethodDefinitionFromTypeNameWithAliasFallback(inferredReceiverTypeName, lookupError);
      }
      if (resolvedDef != nullptr) {
        return resolvedDef;
      }
      std::vector<std::string> receiverPaths = collectionHelperPathCandidates(resolveExprPath(*receiver));
      auto pruneRemovedMapCompatibilityReceiverPaths = [&](const std::string &path) {
        std::string normalizedPath = path;
        if (!normalizedPath.empty() && normalizedPath.front() != '/') {
          if (normalizedPath.rfind("map/", 0) == 0 || normalizedPath.rfind("std/collections/map/", 0) == 0) {
            normalizedPath.insert(normalizedPath.begin(), '/');
          }
        }
        auto isRemovedMapCompatibilityHelper = [](const std::string &suffix) {
          return suffix == "count" || suffix == "contains" || suffix == "tryAt" ||
                 suffix == "at" || suffix == "at_unsafe";
        };
        auto eraseCandidate = [&](const std::string &candidate) {
          for (auto it = receiverPaths.begin(); it != receiverPaths.end();) {
            if (*it == candidate) {
              it = receiverPaths.erase(it);
            } else {
              ++it;
            }
          }
        };
        if (normalizedPath.rfind("/map/", 0) == 0) {
          const std::string suffix = normalizedPath.substr(std::string("/map/").size());
          if (isRemovedMapCompatibilityHelper(suffix)) {
            eraseCandidate("/std/collections/map/" + suffix);
          }
        } else if (normalizedPath.rfind("/std/collections/map/", 0) == 0) {
          const std::string suffix = normalizedPath.substr(std::string("/std/collections/map/").size());
          if (isRemovedMapCompatibilityHelper(suffix)) {
            eraseCandidate("/map/" + suffix);
          }
        }
      };
      pruneRemovedMapCompatibilityReceiverPaths(resolveExprPath(*receiver));
      auto appendUniqueReceiverPath = [&](const std::string &candidate) {
        if (candidate.empty()) {
          return;
        }
        for (const auto &existing : receiverPaths) {
          if (existing == candidate) {
            return;
          }
        }
        receiverPaths.push_back(candidate);
      };
      if (receiverDef != nullptr) {
        const auto resolvedReceiverPaths = collectionHelperPathCandidates(receiverDef->fullPath);
        for (const auto &resolvedReceiverPath : resolvedReceiverPaths) {
          appendUniqueReceiverPath(resolvedReceiverPath);
        }
        pruneRemovedMapCompatibilityReceiverPaths(receiverDef->fullPath);
      }
      for (const auto &receiverPath : receiverPaths) {
        auto receiverDefIt = defMap.find(receiverPath);
        if (receiverDefIt == defMap.end() || receiverDefIt->second == nullptr) {
          continue;
        }
        if (!inferReceiverTypeFromDeclaredReturn(*receiverDefIt->second, typeName)) {
          continue;
        }
        lookupError.clear();
        resolvedDef = resolveMethodDefinitionFromTypeNameWithAliasFallback(typeName, lookupError);
        break;
      }
    }
  }
  if (resolvedDef == nullptr) {
    const bool blocksBuiltinBareVectorCountMethod =
        isSimpleCallName(callExpr, "count") && typeName == "vector";
    const bool blocksBuiltinBareVectorAccessMethod =
        (isSimpleCallName(callExpr, "at") || isSimpleCallName(callExpr, "at_unsafe")) &&
        typeName == "vector";
    const bool blocksBuiltinBareVectorMutatorMethod =
        (isSimpleCallName(callExpr, "push") || isSimpleCallName(callExpr, "pop") ||
         isSimpleCallName(callExpr, "reserve") || isSimpleCallName(callExpr, "clear") ||
         isSimpleCallName(callExpr, "remove_at") || isSimpleCallName(callExpr, "remove_swap")) &&
        typeName == "vector";
    if (allowBuiltinFallback && !blocksBuiltinBareVectorCountMethod &&
        !blocksBuiltinBareVectorAccessMethod && !blocksBuiltinBareVectorMutatorMethod) {
      errorOut = priorError;
      return nullptr;
    }
    errorOut = std::move(lookupError);
    return nullptr;
  }
  return resolvedDef;
}

bool resolveMethodReceiverTypeFromNameExpr(const Expr &receiverNameExpr,
                                           const LocalMap &localsIn,
                                           const std::string &methodName,
                                           std::string &typeNameOut,
                                           std::string &resolvedTypePathOut,
                                           std::string &errorOut) {
  if (receiverNameExpr.kind != Expr::Kind::Name) {
    errorOut = "internal method receiver type resolution requires name expression";
    return false;
  }

  auto it = localsIn.find(receiverNameExpr.name);
  if (it == localsIn.end()) {
    errorOut = "native backend does not know identifier: " + receiverNameExpr.name;
    return false;
  }
  if (!resolveMethodReceiverTypeFromLocalInfo(it->second, typeNameOut, resolvedTypePathOut)) {
    errorOut = "unknown method target for " + methodName;
    return false;
  }
  return true;
}

bool resolveMethodReceiverTarget(const Expr &receiverExpr,
                                 const LocalMap &localsIn,
                                 const std::string &methodName,
                                 const std::unordered_map<std::string, std::string> &importAliases,
                                 const std::unordered_set<std::string> &structNames,
                                 const InferReceiverExprKindFn &inferExprKind,
                                 const ResolveReceiverExprPathFn &resolveExprPath,
                                 std::string &typeNameOut,
                                 std::string &resolvedTypePathOut,
                                 std::string &errorOut) {
  typeNameOut.clear();
  resolvedTypePathOut.clear();

  auto resolveStructTypePathFromName = [&](const std::string &typeName,
                                           const std::string &namespacePrefix) -> std::string {
    if (typeName.empty()) {
      return "";
    }
    if (typeName.front() == '/') {
      return structNames.count(typeName) > 0 ? typeName : "";
    }
    std::string current = namespacePrefix;
    while (true) {
      if (!current.empty()) {
        const std::string scoped = current + "/" + typeName;
        if (structNames.count(scoped) > 0) {
          return scoped;
        }
        if (current.size() > typeName.size()) {
          const size_t start = current.size() - typeName.size();
          if (start > 0 && current[start - 1] == '/' &&
              current.compare(start, typeName.size(), typeName) == 0 &&
              structNames.count(current) > 0) {
            return current;
          }
        }
      } else {
        const std::string root = "/" + typeName;
        if (structNames.count(root) > 0) {
          return root;
        }
      }
      if (current.empty()) {
        break;
      }
      const size_t slash = current.find_last_of('/');
      if (slash == std::string::npos || slash == 0) {
        current.clear();
      } else {
        current.erase(slash);
      }
    }
    auto importIt = importAliases.find(typeName);
    if (importIt != importAliases.end()) {
      return importIt->second;
    }
    return "";
  };

  if (receiverExpr.kind == Expr::Kind::Name) {
    if (resolveMethodReceiverTypeFromNameExpr(
            receiverExpr, localsIn, methodName, typeNameOut, resolvedTypePathOut, errorOut)) {
      return true;
    }
    const std::string resolvedStructType = resolveStructTypePathFromName(receiverExpr.name, receiverExpr.namespacePrefix);
    if (!resolvedStructType.empty()) {
      errorOut.clear();
      resolvedTypePathOut = resolvedStructType;
      return true;
    }
    return false;
  }
  if (receiverExpr.kind == Expr::Kind::Call) {
    auto resolveDereferencedCollectionOrFileErrorReceiver = [&](const Expr &targetExpr) {
      auto classifyLocal = [&](const LocalInfo &localInfo, bool fromArgsPack) -> bool {
        const LocalInfo::Kind receiverKind =
            fromArgsPack ? localInfo.argsPackElementKind : localInfo.kind;
        const bool isReferenceArray = receiverKind == LocalInfo::Kind::Reference && localInfo.referenceToArray;
        const bool isPointerArray = receiverKind == LocalInfo::Kind::Pointer && localInfo.pointerToArray;
        const bool isReferenceVector = receiverKind == LocalInfo::Kind::Reference && localInfo.referenceToVector;
        const bool isPointerVector = receiverKind == LocalInfo::Kind::Pointer && localInfo.pointerToVector;
        const bool isReferenceMap = receiverKind == LocalInfo::Kind::Reference && localInfo.referenceToMap;
        const bool isPointerMap = receiverKind == LocalInfo::Kind::Pointer && localInfo.pointerToMap;
        if (localInfo.isFileError &&
            (receiverKind == LocalInfo::Kind::Reference || receiverKind == LocalInfo::Kind::Pointer)) {
          typeNameOut = "FileError";
          return true;
        }
        if (isReferenceArray || isPointerArray) {
          typeNameOut = "array";
          return true;
        }
        if (isReferenceVector || isPointerVector) {
          typeNameOut = localInfo.isSoaVector ? "soa_vector" : "vector";
          return true;
        }
        if (isReferenceMap || isPointerMap) {
          typeNameOut = "map";
          return true;
        }
        return false;
      };

      if (targetExpr.kind == Expr::Kind::Name) {
        auto localIt = localsIn.find(targetExpr.name);
        return localIt != localsIn.end() && classifyLocal(localIt->second, false);
      }

      std::string derefAccessName;
      if (targetExpr.kind == Expr::Kind::Call && getBuiltinArrayAccessName(targetExpr, derefAccessName) &&
          targetExpr.args.size() == 2 && targetExpr.args.front().kind == Expr::Kind::Name) {
        auto localIt = localsIn.find(targetExpr.args.front().name);
        return localIt != localsIn.end() && localIt->second.isArgsPack &&
               classifyLocal(localIt->second, true);
      }

      return false;
    };

    std::string accessName;
    if (getBuiltinArrayAccessName(receiverExpr, accessName) && receiverExpr.args.size() == 2) {
      const Expr &accessReceiver = receiverExpr.args.front();
      if (accessReceiver.kind == Expr::Kind::Name) {
        auto localIt = localsIn.find(accessReceiver.name);
        if (localIt != localsIn.end() && localIt->second.isArgsPack) {
          if (localIt->second.isFileError &&
              localIt->second.argsPackElementKind == LocalInfo::Kind::Value) {
            typeNameOut = "FileError";
            return true;
          }
          if (!localIt->second.structTypeName.empty()) {
            resolvedTypePathOut = localIt->second.structTypeName;
            return true;
          }
          if (localIt->second.argsPackElementKind == LocalInfo::Kind::Vector) {
            typeNameOut = localIt->second.isSoaVector ? "soa_vector" : "vector";
            return true;
          }
          if (localIt->second.argsPackElementKind == LocalInfo::Kind::Reference &&
              localIt->second.referenceToArray) {
            typeNameOut = "array";
            return true;
          }
          if (localIt->second.argsPackElementKind == LocalInfo::Kind::Pointer &&
              localIt->second.pointerToArray) {
            typeNameOut = "array";
            return true;
          }
          if (localIt->second.argsPackElementKind == LocalInfo::Kind::Reference &&
              localIt->second.referenceToVector) {
            typeNameOut = localIt->second.isSoaVector ? "soa_vector" : "vector";
            return true;
          }
          if (localIt->second.argsPackElementKind == LocalInfo::Kind::Pointer &&
              localIt->second.pointerToVector) {
            typeNameOut = localIt->second.isSoaVector ? "soa_vector" : "vector";
            return true;
          }
          if (localIt->second.argsPackElementKind == LocalInfo::Kind::Reference &&
              localIt->second.referenceToMap) {
            typeNameOut = "map";
            return true;
          }
          if (localIt->second.argsPackElementKind == LocalInfo::Kind::Pointer &&
              localIt->second.pointerToMap) {
            typeNameOut = "map";
            return true;
          }
          if (localIt->second.argsPackElementKind == LocalInfo::Kind::Array) {
            typeNameOut = "array";
            return true;
          }
          if (localIt->second.argsPackElementKind == LocalInfo::Kind::Map) {
            typeNameOut = "map";
            return true;
          }
        }
      }
    }
    if (isSimpleCallName(receiverExpr, "dereference") && receiverExpr.args.size() == 1) {
      const Expr &targetExpr = receiverExpr.args.front();
      if (resolveDereferencedCollectionOrFileErrorReceiver(targetExpr)) {
        return true;
      }
    }
    const bool blocksExplicitVectorAccessKindFallback = isExplicitVectorAccessHelperExpr(receiverExpr);
    const LocalInfo::ValueKind inferredKind =
        (inferExprKind && !blocksExplicitVectorAccessKindFallback)
            ? inferExprKind(receiverExpr, localsIn)
            : LocalInfo::ValueKind::Unknown;
    typeNameOut = resolveMethodReceiverTypeNameFromCallExpr(receiverExpr, inferredKind);
    if (typeNameOut.empty() && !blocksExplicitVectorAccessKindFallback && receiverExpr.isMethodCall &&
        receiverExpr.args.size() == 2) {
      std::string accessName;
      if (getBuiltinArrayAccessName(receiverExpr, accessName) &&
          inferExprKind && inferExprKind(receiverExpr.args.front(), localsIn) == LocalInfo::ValueKind::String) {
        typeNameOut = "i32";
      }
    }
    if (typeNameOut.empty()) {
      resolvedTypePathOut = resolveMethodReceiverStructTypePathFromCallExpr(
          receiverExpr, resolveExprPath(receiverExpr), importAliases, structNames);
    }
    return true;
  }

  typeNameOut = inferExprKind ? typeNameForValueKind(inferExprKind(receiverExpr, localsIn)) : "";
  return true;
}

} // namespace primec::ir_lowerer
