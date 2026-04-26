#include "IrLowererStructReturnPathHelpers.h"

#include "IrLowererHelpers.h"
#include "IrLowererSetupTypeHelpers.h"
#include "IrLowererStructFieldBindingHelpers.h"
#include "IrLowererTemplateTypeParseHelpers.h"

namespace primec::ir_lowerer {

namespace {

bool allowsArrayVectorCompatibilitySuffix(const std::string &suffix) {
  return suffix != "count" && suffix != "capacity" && suffix != "at" && suffix != "at_unsafe" &&
         suffix != "push" && suffix != "pop" && suffix != "reserve" && suffix != "clear" &&
         suffix != "remove_at" && suffix != "remove_swap";
}

bool isRemovedMapCompatibilityHelper(const std::string &suffix) {
  return suffix == "count" || suffix == "contains" || suffix == "tryAt" ||
         suffix == "at" || suffix == "at_unsafe" || suffix == "insert";
}

std::string resolveSpecializedExperimentalVectorReturnPath(
    std::string typeText) {
  typeText = trimTemplateTypeText(typeText);
  while (!typeText.empty()) {
    std::string base;
    std::string argText;
    if (!splitTemplateTypeName(typeText, base, argText)) {
      break;
    }
    base = normalizeDeclaredCollectionTypeBase(trimTemplateTypeText(base));
    std::vector<std::string> args;
    if ((base != "Reference" && base != "Pointer") ||
        !splitTemplateArgs(argText, args) || args.size() != 1) {
      break;
    }
    typeText = trimTemplateTypeText(args.front());
  }

  std::string base;
  std::string argText;
  if (!splitTemplateTypeName(typeText, base, argText)) {
    return "";
  }
  base = normalizeDeclaredCollectionTypeBase(trimTemplateTypeText(base));
  if (base != "vector") {
    return "";
  }

  std::vector<std::string> args;
  if (!splitTemplateArgs(argText, args) || args.size() != 1) {
    return "";
  }

  std::string normalizedArg = trimTemplateTypeText(args.front());
  if (!normalizedArg.empty() && normalizedArg.front() == '/') {
    normalizedArg.erase(normalizedArg.begin());
  }
  return specializedExperimentalVectorStructPathForElementType(normalizedArg);
}

std::string resolveSpecializedExperimentalSoaVectorReturnPath(
    std::string typeText) {
  typeText = trimTemplateTypeText(typeText);
  while (!typeText.empty()) {
    std::string base;
    std::string argText;
    if (!splitTemplateTypeName(typeText, base, argText)) {
      break;
    }
    base = normalizeDeclaredCollectionTypeBase(trimTemplateTypeText(base));
    std::vector<std::string> args;
    if ((base != "Reference" && base != "Pointer") ||
        !splitTemplateArgs(argText, args) || args.size() != 1) {
      break;
    }
    typeText = trimTemplateTypeText(args.front());
  }

  std::string base;
  std::string argText;
  if (!splitTemplateTypeName(typeText, base, argText)) {
    return "";
  }
  base = normalizeDeclaredCollectionTypeBase(trimTemplateTypeText(base));
  if (base != "soa_vector") {
    return "";
  }

  std::vector<std::string> args;
  if (!splitTemplateArgs(argText, args) || args.size() != 1) {
    return "";
  }

  std::string normalizedArg = trimTemplateTypeText(args.front());
  if (!normalizedArg.empty() && normalizedArg.front() == '/') {
    normalizedArg.erase(normalizedArg.begin());
  }
  return specializedExperimentalSoaVectorStructPathForElementType(
      normalizedArg);
}

std::string resolveUniqueStructByLeafName(const std::string &typeName,
                                          const std::unordered_set<std::string> &structNames) {
  if (typeName.empty() || typeName.front() == '/') {
    return "";
  }
  const std::string suffix = "/" + typeName;
  std::string uniqueMatch;
  for (const auto &path : structNames) {
    if (path.size() < suffix.size() ||
        path.compare(path.size() - suffix.size(), suffix.size(), suffix) != 0) {
      continue;
    }
    if (!uniqueMatch.empty() && uniqueMatch != path) {
      return "";
    }
    uniqueMatch = path;
  }
  return uniqueMatch;
}

std::string normalizeCollectionMethodName(std::string methodName) {
  if (!methodName.empty() && methodName.front() == '/') {
    methodName.erase(methodName.begin());
  }
  const std::string vectorPrefix = "vector/";
  const std::string arrayPrefix = "array/";
  const std::string stdVectorPrefix = "std/collections/vector/";
  const std::string mapPrefix = "map/";
  const std::string stdMapPrefix = "std/collections/map/";
  if (methodName.rfind(vectorPrefix, 0) == 0) {
    return methodName.substr(vectorPrefix.size());
  }
  if (methodName.rfind(arrayPrefix, 0) == 0) {
    return methodName.substr(arrayPrefix.size());
  }
  if (methodName.rfind(stdVectorPrefix, 0) == 0) {
    return methodName.substr(stdVectorPrefix.size());
  }
  if (methodName.rfind(mapPrefix, 0) == 0) {
    return methodName.substr(mapPrefix.size());
  }
  if (methodName.rfind(stdMapPrefix, 0) == 0) {
    return methodName.substr(stdMapPrefix.size());
  }
  return methodName;
}

std::string inferBuiltinCollectionReceiverPath(
    const Expr &expr,
    const std::unordered_map<std::string, LayoutFieldBinding> &knownFields) {
  auto normalizeBuiltinCollectionName = [](const std::string &typeName) -> std::string {
    if (typeName == "vector" || typeName == "array" || typeName == "map" || typeName == "string") {
      return "/" + typeName;
    }
    if (typeName.rfind("vector<", 0) == 0) {
      return "/vector";
    }
    if (typeName.rfind("array<", 0) == 0) {
      return "/array";
    }
    if (typeName.rfind("map<", 0) == 0) {
      return "/map";
    }
    return "";
  };

  if (expr.kind == Expr::Kind::Name) {
    auto fieldIt = knownFields.find(expr.name);
    if (fieldIt == knownFields.end()) {
      return "";
    }
    std::string fieldType = fieldIt->second.typeName;
    if ((fieldType == "Reference" || fieldType == "Pointer") && !fieldIt->second.typeTemplateArg.empty()) {
      fieldType = fieldIt->second.typeTemplateArg;
    }
    return normalizeBuiltinCollectionName(fieldType);
  }

  if (expr.kind == Expr::Kind::Call) {
    std::string collectionName;
    if (getBuiltinCollectionName(expr, collectionName)) {
      return normalizeBuiltinCollectionName(collectionName);
    }
  }

  return "";
}

std::vector<std::string> collectionMethodPathCandidates(const std::string &receiverStruct,
                                                        const std::string &methodName,
                                                        const std::string &rawMethodName) {
  if (receiverStruct == "/vector") {
    std::vector<std::string> candidates = {"/std/collections/vector/" + methodName};
    if (allowsArrayVectorCompatibilitySuffix(methodName)) {
      candidates.push_back("/array/" + methodName);
    }
    return candidates;
  }
  if (receiverStruct == "/array") {
    std::vector<std::string> candidates = {
        "/array/" + methodName,
    };
    if (allowsArrayVectorCompatibilitySuffix(methodName)) {
      candidates.push_back("/std/collections/vector/" + methodName);
    }
    return candidates;
  }
  if (receiverStruct == "/map") {
    std::string normalizedRawMethodName = rawMethodName;
    if (!normalizedRawMethodName.empty() && normalizedRawMethodName.front() == '/') {
      normalizedRawMethodName.erase(normalizedRawMethodName.begin());
    }
    if (normalizedRawMethodName.rfind("map/", 0) == 0 &&
        isRemovedMapCompatibilityHelper(
            normalizedRawMethodName.substr(std::string("map/").size()))) {
      return {"/map/" + methodName};
    }
    if (normalizedRawMethodName.rfind("std/collections/map/", 0) == 0 &&
        isRemovedMapCompatibilityHelper(
            normalizedRawMethodName.substr(std::string("std/collections/map/").size()))) {
      return {"/std/collections/map/" + methodName};
    }
    if (isRemovedMapCompatibilityHelper(methodName)) {
      return {"/std/collections/map/" + methodName};
    }
    return {"/std/collections/map/" + methodName, "/map/" + methodName};
  }
  return {receiverStruct + "/" + methodName};
}

std::vector<std::string> collectionHelperPathCandidates(const std::string &path) {
  std::vector<std::string> candidates;
  auto appendUnique = [&](const std::string &candidate) {
    if (candidate.empty()) {
      return;
    }
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
    if (allowsArrayVectorCompatibilitySuffix(suffix)) {
      appendUnique("/std/collections/vector/" + suffix);
    }
  } else if (normalizedPath.rfind("/map/", 0) == 0) {
    const std::string suffix = normalizedPath.substr(std::string("/map/").size());
    if (!isRemovedMapCompatibilityHelper(suffix)) {
      appendUnique("/std/collections/map/" + suffix);
    }
  } else if (normalizedPath.rfind("/std/collections/map/", 0) == 0) {
    const std::string suffix = normalizedPath.substr(std::string("/std/collections/map/").size());
    if (!isRemovedMapCompatibilityHelper(suffix)) {
      appendUnique("/map/" + suffix);
    }
  }
  return candidates;
}

std::string preferCollectionHelperPath(const std::string &path,
                                       const std::unordered_map<std::string, const Definition *> &defMap) {
  std::string preferred = path;
  if (preferred.rfind("/array/", 0) == 0 && defMap.count(preferred) == 0) {
    const std::string suffix = preferred.substr(std::string("/array/").size());
    if (allowsArrayVectorCompatibilitySuffix(suffix)) {
      const std::string stdlibAlias = "/std/collections/vector/" + suffix;
      if (defMap.count(stdlibAlias) > 0) {
        return stdlibAlias;
      }
    }
  }
  if (preferred.rfind("/map/", 0) == 0 && defMap.count(preferred) == 0) {
    const std::string stdlibAlias = "/std/collections/map/" + preferred.substr(std::string("/map/").size());
    if (defMap.count(stdlibAlias) > 0) {
      preferred = stdlibAlias;
    }
  }
  if (preferred.rfind("/std/collections/map/", 0) == 0 && defMap.count(preferred) == 0) {
    const std::string suffix = preferred.substr(std::string("/std/collections/map/").size());
    if (!isRemovedMapCompatibilityHelper(suffix)) {
      const std::string mapAlias = "/map/" + suffix;
      if (defMap.count(mapAlias) > 0) {
        preferred = mapAlias;
      }
    }
  }
  return preferred;
}

std::string inferStructReturnPathFromExprInternal(
    const Expr &expr,
    const std::unordered_map<std::string, LayoutFieldBinding> &knownFields,
    const std::unordered_set<std::string> &structNames,
    const ResolveStructTypePathFn &resolveStructTypePath,
    const ResolveStructLayoutExprPathFn &resolveStructLayoutExprPath,
    const std::unordered_map<std::string, const Definition *> &defMap,
    std::unordered_set<std::string> &visitedDefs);

std::string inferStructReturnPathFromDefinitionInternal(
    const std::string &defPath,
    const std::unordered_set<std::string> &structNames,
    const ResolveStructTypePathFn &resolveStructTypePath,
    const ResolveStructLayoutExprPathFn &resolveStructLayoutExprPath,
    const std::unordered_map<std::string, const Definition *> &defMap,
    std::unordered_set<std::string> &visitedDefs) {
  if (defPath.empty()) {
    return "";
  }
  if (!visitedDefs.insert(defPath).second) {
    return "";
  }
  struct VisitedDefScope {
    std::unordered_set<std::string> &visited;
    std::string path;
    ~VisitedDefScope() {
      visited.erase(path);
    }
  } visitedScope{visitedDefs, defPath};
  auto defIt = defMap.find(defPath);
  if (defIt == defMap.end() || defIt->second == nullptr) {
    return "";
  }

  const Definition &def = *defIt->second;
  for (const auto &transform : def.transforms) {
    if (transform.name != "return" || transform.templateArgs.size() != 1) {
      continue;
    }
    const std::string &returnTypeName = transform.templateArgs.front();
    const std::string specializedVectorResolved =
        resolveSpecializedExperimentalVectorReturnPath(returnTypeName);
    if (structNames.count(specializedVectorResolved) > 0) {
      return specializedVectorResolved;
    }
    const std::string specializedSoaResolved =
        resolveSpecializedExperimentalSoaVectorReturnPath(returnTypeName);
    if (structNames.count(specializedSoaResolved) > 0) {
      return specializedSoaResolved;
    }
    std::string resolved = resolveStructTypePath(returnTypeName, def.namespacePrefix);
    if (structNames.count(resolved) == 0 && !returnTypeName.empty() && returnTypeName[0] != '/') {
      const std::string rootResolved = resolveStructTypePath(returnTypeName, "");
      if (structNames.count(rootResolved) > 0) {
        resolved = rootResolved;
      } else if (!def.namespacePrefix.empty()) {
        const std::size_t slash = def.namespacePrefix.find_last_of('/');
        if (slash != std::string::npos && slash > 0) {
          const std::string parentNamespace = def.namespacePrefix.substr(0, slash);
          const std::string parentResolved = resolveStructTypePath(returnTypeName, parentNamespace);
          if (structNames.count(parentResolved) > 0) {
            resolved = parentResolved;
          }
        }
      }
      if (structNames.count(resolved) == 0) {
        const std::string uniqueResolved = resolveUniqueStructByLeafName(returnTypeName, structNames);
        if (!uniqueResolved.empty()) {
          resolved = uniqueResolved;
        }
      }
    }
    if (structNames.count(resolved) > 0) {
      return resolved;
    }
    break;
  }

  auto inferFromReturnValue = [&](const Expr &valueExpr,
                                  const std::unordered_map<std::string, LayoutFieldBinding> &knownFields) {
    return inferStructReturnPathFromExprInternal(
        valueExpr, knownFields, structNames, resolveStructTypePath, resolveStructLayoutExprPath, defMap, visitedDefs);
  };

  if (def.returnExpr.has_value()) {
    const std::unordered_map<std::string, LayoutFieldBinding> noFields;
    const std::string inferred = inferFromReturnValue(*def.returnExpr, noFields);
    if (!inferred.empty()) {
      return inferred;
    }
  }

  std::string inferred;
  std::unordered_map<std::string, LayoutFieldBinding> knownLocalBindings;
  const Expr *lastValueStmt = nullptr;
  for (const auto &stmt : def.statements) {
    if (stmt.isBinding) {
      LayoutFieldBinding bindingInfo;
      std::string bindingError;
      if (resolveLayoutFieldBinding(def,
                                    stmt,
                                    knownLocalBindings,
                                    structNames,
                                    resolveStructTypePath,
                                    resolveStructLayoutExprPath,
                                    defMap,
                                    bindingInfo,
                                    bindingError)) {
        knownLocalBindings[stmt.name] = std::move(bindingInfo);
      }
      continue;
    }
    lastValueStmt = &stmt;
    if (!isReturnCall(stmt) || stmt.args.size() != 1) {
      continue;
    }
    const std::string candidate = inferFromReturnValue(stmt.args.front(), knownLocalBindings);
    if (candidate.empty()) {
      continue;
    }
    if (inferred.empty()) {
      inferred = candidate;
      continue;
    }
    if (candidate != inferred) {
      return "";
    }
  }

  if (inferred.empty() && lastValueStmt != nullptr) {
    return inferFromReturnValue(*lastValueStmt, knownLocalBindings);
  }

  return inferred;
}

std::string inferStructReturnPathFromExprInternal(
    const Expr &expr,
    const std::unordered_map<std::string, LayoutFieldBinding> &knownFields,
    const std::unordered_set<std::string> &structNames,
    const ResolveStructTypePathFn &resolveStructTypePath,
    const ResolveStructLayoutExprPathFn &resolveStructLayoutExprPath,
    const std::unordered_map<std::string, const Definition *> &defMap,
    std::unordered_set<std::string> &visitedDefs) {
  if (expr.kind == Expr::Kind::Name) {
    auto fieldIt = knownFields.find(expr.name);
    if (fieldIt == knownFields.end()) {
      return "";
    }
    std::string fieldType = fieldIt->second.typeName;
    if ((fieldType == "Reference" || fieldType == "Pointer") && !fieldIt->second.typeTemplateArg.empty()) {
      fieldType = fieldIt->second.typeTemplateArg;
    }
    const std::string resolved = resolveStructTypePath(fieldType, expr.namespacePrefix);
    return structNames.count(resolved) > 0 ? resolved : "";
  }
  if (expr.kind != Expr::Kind::Call) {
    return "";
  }
  if (isMatchCall(expr)) {
    Expr lowered;
    std::string loweredError;
    if (!lowerMatchToIf(expr, lowered, loweredError)) {
      return "";
    }
    return inferStructReturnPathFromExprInternal(
        lowered, knownFields, structNames, resolveStructTypePath, resolveStructLayoutExprPath, defMap, visitedDefs);
  }
  if (isIfCall(expr) && expr.args.size() == 3) {
    const Expr *thenValue = getEnvelopeValueExpr(expr.args[1], true);
    const Expr *elseValue = getEnvelopeValueExpr(expr.args[2], true);
    const Expr &thenExpr = thenValue ? *thenValue : expr.args[1];
    const Expr &elseExpr = elseValue ? *elseValue : expr.args[2];
    const std::string thenPath = inferStructReturnPathFromExprInternal(
        thenExpr, knownFields, structNames, resolveStructTypePath, resolveStructLayoutExprPath, defMap, visitedDefs);
    if (thenPath.empty()) {
      return "";
    }
    const std::string elsePath = inferStructReturnPathFromExprInternal(
        elseExpr, knownFields, structNames, resolveStructTypePath, resolveStructLayoutExprPath, defMap, visitedDefs);
    return thenPath == elsePath ? thenPath : "";
  }
  if (const Expr *valueExpr = getEnvelopeValueExpr(expr, false)) {
    if (isReturnCall(*valueExpr) && !valueExpr->args.empty()) {
      return inferStructReturnPathFromExprInternal(valueExpr->args.front(),
                                                   knownFields,
                                                   structNames,
                                                   resolveStructTypePath,
                                                   resolveStructLayoutExprPath,
                                                   defMap,
                                                   visitedDefs);
    }
    return inferStructReturnPathFromExprInternal(
        *valueExpr, knownFields, structNames, resolveStructTypePath, resolveStructLayoutExprPath, defMap, visitedDefs);
  }
  if (expr.isMethodCall) {
    if (expr.args.empty()) {
      return "";
    }
    std::string receiverStruct = inferBuiltinCollectionReceiverPath(expr.args.front(), knownFields);
    if (receiverStruct.empty()) {
      receiverStruct = inferStructReturnPathFromExprInternal(expr.args.front(),
                                                             knownFields,
                                                             structNames,
                                                             resolveStructTypePath,
                                                             resolveStructLayoutExprPath,
                                                             defMap,
                                                             visitedDefs);
    }
    if (receiverStruct.empty()) {
      return "";
    }
    std::string rawMethodName = expr.name;
    if (!rawMethodName.empty() && rawMethodName.front() == '/') {
      rawMethodName.erase(rawMethodName.begin());
    }
    const std::string methodName = normalizeCollectionMethodName(expr.name);
    std::vector<std::string> candidates = collectionMethodPathCandidates(receiverStruct, methodName, rawMethodName);
    if ((receiverStruct == "/vector" || receiverStruct == "/array" || receiverStruct == "/string") &&
        (methodName == "at" || methodName == "at_unsafe")) {
      const std::string canonicalCandidate = "/std/collections/vector/" + methodName;
      for (auto it = candidates.begin(); it != candidates.end();) {
        if (*it == canonicalCandidate) {
          it = candidates.erase(it);
        } else {
          ++it;
        }
      }
    }
    for (const auto &candidate : candidates) {
      if (const std::string inferred = inferStructReturnPathFromDefinitionInternal(
              candidate, structNames, resolveStructTypePath, resolveStructLayoutExprPath, defMap, visitedDefs);
          !inferred.empty()) {
        return inferred;
      }
    }
    return "";
  }

  auto resolvedCandidates = collectionHelperPathCandidates(resolveStructLayoutExprPath(expr));
  if (!expr.isMethodCall && !expr.args.empty()) {
    std::string normalizedPath = resolveStructLayoutExprPath(expr);
    if (!normalizedPath.empty() && normalizedPath.front() != '/') {
      if (normalizedPath.rfind("vector/", 0) == 0 || normalizedPath.rfind("std/collections/vector/", 0) == 0) {
        normalizedPath.insert(normalizedPath.begin(), '/');
      }
    }
    if (normalizedPath.rfind("/std/collections/vector/", 0) == 0) {
      const std::string suffix = normalizedPath.substr(std::string("/std/collections/vector/").size());
      if (suffix == "at" || suffix == "at_unsafe") {
        size_t receiverIndex = 0;
        if (hasNamedArguments(expr.argNames)) {
          bool foundValues = false;
          for (size_t i = 0; i < expr.args.size(); ++i) {
            if (i < expr.argNames.size() && expr.argNames[i].has_value() && *expr.argNames[i] == "values") {
              receiverIndex = i;
              foundValues = true;
              break;
            }
          }
          if (!foundValues) {
            receiverIndex = 0;
          }
        }
        if (receiverIndex < expr.args.size()) {
          std::string receiverStruct = inferBuiltinCollectionReceiverPath(expr.args[receiverIndex], knownFields);
          if (receiverStruct.empty()) {
            receiverStruct = inferStructReturnPathFromExprInternal(expr.args[receiverIndex],
                                                                   knownFields,
                                                                   structNames,
                                                                   resolveStructTypePath,
                                                                   resolveStructLayoutExprPath,
                                                                   defMap,
                                                                   visitedDefs);
          }
          if (receiverStruct == "/vector" || receiverStruct == "/array" || receiverStruct == "/string") {
            const std::string canonicalCandidate = "/std/collections/vector/" + suffix;
            for (auto it = resolvedCandidates.begin(); it != resolvedCandidates.end();) {
              if (*it == canonicalCandidate) {
                it = resolvedCandidates.erase(it);
              } else {
                ++it;
              }
            }
            if (resolvedCandidates.empty()) {
              return "";
            }
          }
        }
      }
    }
  }
  for (const auto &candidate : resolvedCandidates) {
    if (structNames.count(candidate) > 0) {
      return candidate;
    }
  }
  for (const auto &candidate : resolvedCandidates) {
    if (const std::string inferred = inferStructReturnPathFromDefinitionInternal(
            candidate, structNames, resolveStructTypePath, resolveStructLayoutExprPath, defMap, visitedDefs);
        !inferred.empty()) {
      return inferred;
    }
  }
  if (resolvedCandidates.empty()) {
    std::string resolved = resolveStructLayoutExprPath(expr);
    if (structNames.count(resolved) > 0) {
      return resolved;
    }
    resolved = preferCollectionHelperPath(resolved, defMap);
    return inferStructReturnPathFromDefinitionInternal(
        resolved, structNames, resolveStructTypePath, resolveStructLayoutExprPath, defMap, visitedDefs);
  }
  return "";
}

} // namespace

std::string inferStructReturnPathFromDefinition(
    const std::string &defPath,
    const std::unordered_set<std::string> &structNames,
    const ResolveStructTypePathFn &resolveStructTypePath,
    const ResolveStructLayoutExprPathFn &resolveStructLayoutExprPath,
    const std::unordered_map<std::string, const Definition *> &defMap) {
  std::unordered_set<std::string> visitedDefs;
  return inferStructReturnPathFromDefinitionInternal(
      defPath, structNames, resolveStructTypePath, resolveStructLayoutExprPath, defMap, visitedDefs);
}

std::string inferStructReturnPathFromExpr(
    const Expr &expr,
    const std::unordered_map<std::string, LayoutFieldBinding> &knownFields,
    const std::unordered_set<std::string> &structNames,
    const ResolveStructTypePathFn &resolveStructTypePath,
    const ResolveStructLayoutExprPathFn &resolveStructLayoutExprPath,
    const std::unordered_map<std::string, const Definition *> &defMap) {
  std::unordered_set<std::string> visitedDefs;
  return inferStructReturnPathFromExprInternal(
      expr, knownFields, structNames, resolveStructTypePath, resolveStructLayoutExprPath, defMap, visitedDefs);
}

} // namespace primec::ir_lowerer
