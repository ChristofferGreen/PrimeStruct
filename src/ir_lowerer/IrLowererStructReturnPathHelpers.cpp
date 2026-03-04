#include "IrLowererStructReturnPathHelpers.h"

#include "IrLowererHelpers.h"

namespace primec::ir_lowerer {

namespace {

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
    }
    if (structNames.count(resolved) > 0) {
      return resolved;
    }
    break;
  }

  auto inferFromReturnValue = [&](const Expr &valueExpr) {
    const std::unordered_map<std::string, LayoutFieldBinding> noFields;
    return inferStructReturnPathFromExprInternal(
        valueExpr, noFields, structNames, resolveStructTypePath, resolveStructLayoutExprPath, defMap, visitedDefs);
  };

  if (def.returnExpr.has_value()) {
    const std::string inferred = inferFromReturnValue(*def.returnExpr);
    if (!inferred.empty()) {
      return inferred;
    }
  }

  std::string inferred;
  for (const auto &stmt : def.statements) {
    if (!isReturnCall(stmt) || stmt.args.size() != 1) {
      continue;
    }
    const std::string candidate = inferFromReturnValue(stmt.args.front());
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
    const std::string receiverStruct = inferStructReturnPathFromExprInternal(expr.args.front(),
                                                                              knownFields,
                                                                              structNames,
                                                                              resolveStructTypePath,
                                                                              resolveStructLayoutExprPath,
                                                                              defMap,
                                                                              visitedDefs);
    if (receiverStruct.empty()) {
      return "";
    }
    return inferStructReturnPathFromDefinitionInternal(
        receiverStruct + "/" + expr.name, structNames, resolveStructTypePath, resolveStructLayoutExprPath, defMap, visitedDefs);
  }

  const std::string resolved = resolveStructLayoutExprPath(expr);
  if (structNames.count(resolved) > 0) {
    return resolved;
  }
  return inferStructReturnPathFromDefinitionInternal(
      resolved, structNames, resolveStructTypePath, resolveStructLayoutExprPath, defMap, visitedDefs);
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
