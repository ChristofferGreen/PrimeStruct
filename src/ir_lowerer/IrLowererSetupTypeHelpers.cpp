#include "IrLowererSetupTypeHelpers.h"

#include "IrLowererHelpers.h"
#include "IrLowererTemplateTypeParseHelpers.h"

namespace primec::ir_lowerer {

namespace {

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
    return true;
  }
  if (normalized.rfind(arrayPrefix, 0) == 0) {
    helperNameOut = normalized.substr(arrayPrefix.size());
    if (helperNameOut == "count") {
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

std::vector<std::string> collectionHelperPathCandidates(const std::string &path);

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

  std::string normalizedPath = path;
  if (!normalizedPath.empty() && normalizedPath.front() != '/') {
    if (normalizedPath.rfind("array/", 0) == 0 || normalizedPath.rfind("vector/", 0) == 0 ||
        normalizedPath.rfind("std/collections/vector/", 0) == 0 || normalizedPath.rfind("map/", 0) == 0 ||
        normalizedPath.rfind("std/collections/map/", 0) == 0) {
      normalizedPath.insert(normalizedPath.begin(), '/');
    }
  }

  appendUnique(path);
  appendUnique(normalizedPath);
  if (normalizedPath.rfind("/array/", 0) == 0) {
    const std::string suffix = normalizedPath.substr(std::string("/array/").size());
    appendUnique("/vector/" + suffix);
    appendUnique("/std/collections/vector/" + suffix);
  } else if (normalizedPath.rfind("/vector/", 0) == 0) {
    const std::string suffix = normalizedPath.substr(std::string("/vector/").size());
    appendUnique("/std/collections/vector/" + suffix);
    appendUnique("/array/" + suffix);
  } else if (normalizedPath.rfind("/std/collections/vector/", 0) == 0) {
    const std::string suffix = normalizedPath.substr(std::string("/std/collections/vector/").size());
    appendUnique("/vector/" + suffix);
    appendUnique("/array/" + suffix);
  } else if (normalizedPath.rfind("/map/", 0) == 0) {
    appendUnique("/std/collections/map/" + normalizedPath.substr(std::string("/map/").size()));
  } else if (normalizedPath.rfind("/std/collections/map/", 0) == 0) {
    appendUnique("/map/" + normalizedPath.substr(std::string("/std/collections/map/").size()));
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

bool inferDeclaredReturnCollection(const Definition &definition,
                                   std::string &collectionNameOut,
                                   std::vector<std::string> &collectionArgsOut) {
  collectionNameOut.clear();
  collectionArgsOut.clear();
  for (const auto &transform : definition.transforms) {
    if (transform.name != "return" || transform.templateArgs.size() != 1) {
      continue;
    }
    std::string base;
    std::string argText;
    if (!splitTemplateTypeName(transform.templateArgs.front(), base, argText)) {
      return false;
    }
    if (!splitTemplateArgs(argText, collectionArgsOut)) {
      return false;
    }
    if ((base == "array" || base == "vector" || base == "soa_vector") &&
        collectionArgsOut.size() == 1) {
      collectionNameOut = base;
      return true;
    }
    if (base == "map" && collectionArgsOut.size() == 2) {
      collectionNameOut = base;
      return true;
    }
    return false;
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
  const bool isBuiltinVectorMutatorCall =
      isVectorBuiltinName(callExpr, "push") || isVectorBuiltinName(callExpr, "pop") ||
      isVectorBuiltinName(callExpr, "reserve") || isVectorBuiltinName(callExpr, "clear") ||
      isVectorBuiltinName(callExpr, "remove_at") || isVectorBuiltinName(callExpr, "remove_swap");
  const bool allowBuiltinFallback =
      isBuiltinCountOrCapacityCall || isBuiltinVectorMutatorCall ||
      (isArrayCountCall && isArrayCountCall(callExpr, localsIn)) ||
      (isVectorCapacityCall && isVectorCapacityCall(callExpr, localsIn)) || isBuiltinAccessCall;
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
  if (localInfo.kind == LocalInfo::Kind::Reference && localInfo.referenceToArray) {
    if (!localInfo.structTypeName.empty()) {
      resolvedTypePathOut = localInfo.structTypeName;
    } else {
      typeNameOut = "array";
    }
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

  std::string resolved = resolvedReceiverPath;
  auto importIt = importAliases.find(receiverCallExpr.name);
  if (structNames.count(resolved) == 0 && importIt != importAliases.end()) {
    resolved = importIt->second;
  }
  if (structNames.count(resolved) > 0) {
    return resolved;
  }
  return "";
}

const Definition *resolveMethodDefinitionFromReceiverTarget(
    const std::string &methodName,
    const std::string &typeName,
    const std::string &resolvedTypePath,
    const std::unordered_map<std::string, const Definition *> &defMap,
    std::string &errorOut) {
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
  }
  auto findMethodDefinitionByPath = [&](const std::string &path) -> const Definition * {
    auto defIt = defMap.find(path);
    if (defIt != defMap.end()) {
      return defIt->second;
    }
    if (path.rfind("/array/", 0) == 0) {
      const std::string suffix = path.substr(std::string("/array/").size());
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
    if (path.rfind("/vector/", 0) == 0) {
      const std::string suffix = path.substr(std::string("/vector/").size());
      const std::string stdlibAlias = "/std/collections/vector/" + suffix;
      defIt = defMap.find(stdlibAlias);
      if (defIt != defMap.end()) {
        return defIt->second;
      }
      const std::string arrayAlias = "/array/" + suffix;
      defIt = defMap.find(arrayAlias);
      if (defIt != defMap.end()) {
        return defIt->second;
      }
    }
    if (path.rfind("/std/collections/vector/", 0) == 0) {
      const std::string suffix = path.substr(std::string("/std/collections/vector/").size());
      const std::string vectorAlias = "/vector/" + suffix;
      defIt = defMap.find(vectorAlias);
      if (defIt != defMap.end()) {
        return defIt->second;
      }
      const std::string arrayAlias = "/array/" + suffix;
      defIt = defMap.find(arrayAlias);
      if (defIt != defMap.end()) {
        return defIt->second;
      }
    }
    if (path.rfind("/map/", 0) == 0) {
      const std::string stdlibAlias = "/std/collections/map/" + path.substr(std::string("/map/").size());
      defIt = defMap.find(stdlibAlias);
      if (defIt != defMap.end()) {
        return defIt->second;
      }
    }
    return nullptr;
  };
  if (!resolvedTypePath.empty()) {
    const std::string resolved = resolvedTypePath + "/" + normalizedMethodName;
    const Definition *resolvedDef = findMethodDefinitionByPath(resolved);
    if (resolvedDef == nullptr) {
      errorOut = "unknown method: " + resolved;
      return nullptr;
    }
    return resolvedDef;
  }

  if (typeName.empty()) {
    errorOut = "unknown method target for " + normalizedMethodName;
    return nullptr;
  }

  const std::string resolved = "/" + typeName + "/" + normalizedMethodName;
  const Definition *resolvedDef = findMethodDefinitionByPath(resolved);
  if (resolvedDef == nullptr) {
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

  const auto candidates = collectionHelperPathCandidates(resolveExprPath(callExpr));
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
                                      bool *methodResolvedOut) {
  kindOut = LocalInfo::ValueKind::Unknown;
  if (methodResolvedOut != nullptr) {
    *methodResolvedOut = false;
  }

  if (callExpr.kind != Expr::Kind::Call || callExpr.isMethodCall) {
    return false;
  }
  const bool isCountCall = (isVectorBuiltinName(callExpr, "count") || isMapBuiltinName(callExpr, "count")) &&
                           callExpr.args.size() == 1;
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
  if (isAccessCall) {
    expectedArgCount = 2u;
  } else if (isVectorMutatorCall) {
    expectedArgCount = expectedVectorMutatorArgCount();
  }
  const bool isSoaFieldHelperCall =
      callExpr.args.size() == 1 && !isCountCall && isSoaVectorReceiverExpr(callExpr.args.front(), localsIn);
  if ((!isCountCall && !isAccessCall && !isSoaFieldHelperCall && !isVectorMutatorCall) ||
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
    if (candidate.kind != Expr::Kind::Name) {
      return false;
    }
    auto it = localsIn.find(candidate.name);
    if (it == localsIn.end()) {
      return false;
    }
    const LocalInfo &info = it->second;
    return info.kind == LocalInfo::Kind::Array || info.kind == LocalInfo::Kind::Vector || info.kind == LocalInfo::Kind::Map ||
           (info.kind == LocalInfo::Kind::Reference && info.referenceToArray) || info.isSoaVector ||
           (info.kind == LocalInfo::Kind::Value && info.valueKind == LocalInfo::ValueKind::String);
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
    return info.kind == LocalInfo::Kind::Vector || info.isSoaVector;
  };
  auto isKnownLocalName = [&](const Expr &candidate) -> bool {
    if (candidate.kind != Expr::Kind::Name) {
      return false;
    }
    return localsIn.find(candidate.name) != localsIn.end();
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
    if (isVectorMutatorCall || isAccessCall) {
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
      isAccessCall && !hasNamedArgsValue && callExpr.args.size() > 1 &&
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

  for (size_t receiverIndex : receiverIndices) {
    Expr methodExpr = buildMethodExprForReceiverIndex(receiverIndex);
    bool methodResolved = false;
    if (resolveMethodCallReturnKind(methodExpr,
                                    localsIn,
                                    resolveMethodCallDefinition,
                                    getReturnInfo,
                                    requireArrayReturn,
                                    kindOut,
                                    &methodResolved)) {
      if (methodResolvedOut != nullptr) {
        *methodResolvedOut = true;
      }
      return true;
    }
    if (methodResolved) {
      if (methodResolvedOut != nullptr) {
        *methodResolvedOut = true;
      }
      return false;
    }
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
  std::string accessName;
  const bool isBuiltinAccessCall = getBuiltinArrayAccessName(callExpr, accessName) && callExpr.args.size() == 2;
  const bool isBuiltinCountOrCapacityCall =
      isVectorBuiltinName(callExpr, "count") || isMapBuiltinName(callExpr, "count") ||
      isVectorBuiltinName(callExpr, "capacity");
  const bool isBuiltinVectorMutatorCall =
      isVectorBuiltinName(callExpr, "push") || isVectorBuiltinName(callExpr, "pop") ||
      isVectorBuiltinName(callExpr, "reserve") || isVectorBuiltinName(callExpr, "clear") ||
      isVectorBuiltinName(callExpr, "remove_at") || isVectorBuiltinName(callExpr, "remove_swap");
  const bool allowBuiltinFallback =
      isBuiltinCountOrCapacityCall || isBuiltinVectorMutatorCall ||
      (isArrayCountCall && isArrayCountCall(callExpr, localsIn)) ||
      (isVectorCapacityCall && isVectorCapacityCall(callExpr, localsIn)) || isBuiltinAccessCall;

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
  if (resolvedDef == nullptr && typeName.empty() && resolvedTypePath.empty() &&
      receiver->kind == Expr::Kind::Call) {
    std::string nestedError = lookupError;
    const Definition *receiverMethodDef = resolveMethodCallDefinitionFromExpr(*receiver,
                                                                              localsIn,
                                                                              isArrayCountCall,
                                                                              isVectorCapacityCall,
                                                                              isEntryArgsName,
                                                                              importAliases,
                                                                              structNames,
                                                                              inferExprKind,
                                                                              resolveExprPath,
                                                                              defMap,
                                                                              nestedError);
    if (receiverMethodDef != nullptr && inferReceiverTypeFromDeclaredReturn(*receiverMethodDef, typeName)) {
      lookupError.clear();
      resolvedDef = resolveMethodDefinitionFromReceiverTarget(callExpr.name, typeName, "", defMap, lookupError);
    } else {
      std::vector<std::string> receiverPaths = collectionHelperPathCandidates(resolveExprPath(*receiver));
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
      if (receiverMethodDef != nullptr) {
        const auto resolvedReceiverPaths = collectionHelperPathCandidates(receiverMethodDef->fullPath);
        for (const auto &resolvedReceiverPath : resolvedReceiverPaths) {
          appendUniqueReceiverPath(resolvedReceiverPath);
        }
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
        resolvedDef = resolveMethodDefinitionFromReceiverTarget(callExpr.name, typeName, "", defMap, lookupError);
        break;
      }
    }
  }
  if (resolvedDef == nullptr) {
    if (allowBuiltinFallback) {
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

  if (receiverExpr.kind == Expr::Kind::Name) {
    return resolveMethodReceiverTypeFromNameExpr(
        receiverExpr, localsIn, methodName, typeNameOut, resolvedTypePathOut, errorOut);
  }
  if (receiverExpr.kind == Expr::Kind::Call) {
    typeNameOut = resolveMethodReceiverTypeNameFromCallExpr(receiverExpr, inferExprKind(receiverExpr, localsIn));
    if (typeNameOut.empty()) {
      resolvedTypePathOut = resolveMethodReceiverStructTypePathFromCallExpr(
          receiverExpr, resolveExprPath(receiverExpr), importAliases, structNames);
    }
    return true;
  }

  typeNameOut = typeNameForValueKind(inferExprKind(receiverExpr, localsIn));
  return true;
}

} // namespace primec::ir_lowerer
