#include "IrLowererUninitializedTypeHelpers.h"

#include <functional>
#include <unordered_set>

#include "IrLowererHelpers.h"

namespace primec::ir_lowerer {

std::string inferStructPathFromCallTargetWithFieldBindingIndex(
    const Expr &expr,
    const InferStructExprPathFn &resolveExprPath,
    const UninitializedFieldBindingIndex &fieldIndex,
    const InferDefinitionStructReturnPathFn &inferDefinitionStructReturnPath) {
  return inferStructPathFromCallTarget(
      expr,
      resolveExprPath,
      [&](const std::string &resolvedPath) {
        return hasUninitializedFieldBindingsForStructPath(fieldIndex, resolvedPath);
      },
      inferDefinitionStructReturnPath);
}

std::string inferStructPathFromCallTargetWithFieldBindingIndexAndVisited(
    const Expr &expr,
    const InferStructExprPathFn &resolveExprPath,
    const UninitializedFieldBindingIndex &fieldIndex,
    const InferDefinitionStructReturnPathWithVisitedFn &inferDefinitionStructReturnPath,
    std::unordered_set<std::string> &visitedDefs) {
  return inferStructPathFromCallTargetWithFieldBindingIndex(
      expr,
      resolveExprPath,
      fieldIndex,
      [&](const std::string &resolvedPath) {
        return inferDefinitionStructReturnPath(resolvedPath, visitedDefs);
      });
}

std::string inferStructReturnPathFromDefinitionMapWithVisited(
    const std::string &defPath,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const ResolveStructTypeNameFn &resolveStructTypeName,
    const InferStructReturnExprWithVisitedFn &inferStructReturnExprPath,
    std::unordered_set<std::string> &visitedDefs) {
  if (defPath.empty()) {
    return "";
  }
  if (!visitedDefs.insert(defPath).second) {
    return "";
  }
  const Definition *resolvedDef = resolveDefinitionByPath(defMap, defPath);
  if (resolvedDef == nullptr) {
    return "";
  }
  return inferStructReturnPathFromDefinition(
      *resolvedDef,
      resolveStructTypeName,
      [&](const Expr &expr) { return inferStructReturnExprPath(expr, visitedDefs); });
}

std::string inferStructReturnPathFromDefinitionMap(
    const std::string &defPath,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const ResolveStructTypeNameFn &resolveStructTypeName,
    const InferStructReturnExprWithVisitedFn &inferStructReturnExprPath) {
  std::unordered_set<std::string> visitedDefs;
  return inferStructReturnPathFromDefinitionMapWithVisited(
      defPath, defMap, resolveStructTypeName, inferStructReturnExprPath, visitedDefs);
}

std::string inferStructReturnPathFromDefinitionMapByCallTargetWithFieldIndexWithVisited(
    const std::string &defPath,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const ResolveStructTypeNameFn &resolveStructTypeName,
    const InferStructExprPathFn &resolveExprPath,
    const UninitializedFieldBindingIndex &fieldIndex,
    std::unordered_set<std::string> &visitedDefs) {
  return inferStructReturnPathFromDefinitionMapWithVisited(
      defPath,
      defMap,
      resolveStructTypeName,
      [&](const Expr &expr, std::unordered_set<std::string> &visitedIn) {
        return inferStructPathFromCallTargetWithFieldBindingIndexAndVisited(
            expr,
            resolveExprPath,
            fieldIndex,
            [&](const std::string &nestedDefPath, std::unordered_set<std::string> &nestedVisited) {
              return inferStructReturnPathFromDefinitionMapByCallTargetWithFieldIndexWithVisited(
                  nestedDefPath, defMap, resolveStructTypeName, resolveExprPath, fieldIndex, nestedVisited);
            },
            visitedIn);
      },
      visitedDefs);
}

std::string inferStructReturnPathFromDefinitionMapByCallTargetWithFieldIndex(
    const std::string &defPath,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const ResolveStructTypeNameFn &resolveStructTypeName,
    const InferStructExprPathFn &resolveExprPath,
    const UninitializedFieldBindingIndex &fieldIndex) {
  std::unordered_set<std::string> visitedDefs;
  return inferStructReturnPathFromDefinitionMapByCallTargetWithFieldIndexWithVisited(
      defPath, defMap, resolveStructTypeName, resolveExprPath, fieldIndex, visitedDefs);
}

std::string inferStructExprPathFromDefinitionMapByCallTargetWithFieldIndex(
    const Expr &expr,
    const LocalMap &localsIn,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const ResolveStructTypeNameFn &resolveStructTypeName,
    const InferStructExprPathFn &resolveExprPath,
    const UninitializedFieldBindingIndex &fieldIndex,
    const ResolveStructFieldSlotFn &resolveStructFieldSlot) {
  std::function<std::string(const Expr &, const LocalMap &)> inferStructExprPath;
  std::unordered_set<std::string> visitedDefs;
  inferStructExprPath = [&](const Expr &exprIn, const LocalMap &localsInExpr) -> std::string {
    const std::string nameStructPath = inferStructPathFromNameExpr(exprIn, localsInExpr);
    if (!nameStructPath.empty()) {
      return nameStructPath;
    }
    if (exprIn.kind == Expr::Kind::Name) {
      std::string resolvedTypePath;
      if (resolveStructTypeName(exprIn.name, exprIn.namespacePrefix, resolvedTypePath)) {
        return resolvedTypePath;
      }
      return "";
    }
    if (exprIn.kind == Expr::Kind::Call) {
      if (!exprIn.isMethodCall && isSimpleCallName(exprIn, "location") && exprIn.args.size() == 1) {
        return inferStructExprPath(exprIn.args.front(), localsInExpr);
      }
      if (!exprIn.isMethodCall && isSimpleCallName(exprIn, "dereference") && exprIn.args.size() == 1) {
        return inferStructExprPath(exprIn.args.front(), localsInExpr);
      }
      if (!exprIn.isMethodCall && isSimpleCallName(exprIn, "try") && exprIn.args.size() == 1) {
        const Expr &resultExpr = exprIn.args.front();
        if (resultExpr.kind == Expr::Kind::Name) {
          auto localIt = localsInExpr.find(resultExpr.name);
          if (localIt != localsInExpr.end() && !localIt->second.resultValueStructType.empty()) {
            return localIt->second.resultValueStructType;
          }
        }
        if (resultExpr.kind == Expr::Kind::Call && resultExpr.isMethodCall && resultExpr.args.size() == 2 &&
            resultExpr.args.front().kind == Expr::Kind::Name && resultExpr.args.front().name == "Result" &&
            resultExpr.name == "ok") {
          return inferStructExprPath(resultExpr.args[1], localsInExpr);
        }
      }
      if (exprIn.isFieldAccess && exprIn.args.size() == 1) {
        const std::string receiverStruct = inferStructExprPath(exprIn.args.front(), localsInExpr);
        if (!receiverStruct.empty()) {
          auto fieldIt = fieldIndex.find(receiverStruct);
          if (fieldIt != fieldIndex.end()) {
            for (const auto &field : fieldIt->second) {
              if (!field.isStatic || field.name != exprIn.name) {
                continue;
              }
              std::string staticFieldStruct;
              if (resolveStructTypeName(field.typeName, exprIn.namespacePrefix, staticFieldStruct)) {
                return staticFieldStruct;
              }
              return "";
            }
          }
        }
      }
      std::string accessName;
      if (getBuiltinArrayAccessName(exprIn, accessName) && exprIn.args.size() == 2) {
        const Expr &accessReceiver = exprIn.args.front();
        if (accessReceiver.kind == Expr::Kind::Name) {
          auto receiverIt = localsInExpr.find(accessReceiver.name);
          if (receiverIt != localsInExpr.end() && receiverIt->second.isArgsPack &&
              !receiverIt->second.structTypeName.empty()) {
            return receiverIt->second.structTypeName;
          }
        }
      }
      const std::string fieldAccessStruct = inferStructPathFromFieldAccessCall(
          exprIn, localsInExpr, inferStructExprPath, resolveStructFieldSlot);
      if (!fieldAccessStruct.empty() || exprIn.isFieldAccess) {
        return fieldAccessStruct;
      }
      if (!exprIn.isMethodCall) {
        const std::string resolvedPath = resolveExprPath(exprIn);
        auto defIt = defMap.find(resolvedPath);
        if (defIt != defMap.end() && defIt->second != nullptr) {
          if (isStructDefinition(*defIt->second)) {
            return resolvedPath;
          }
          const std::string indexedStruct = inferStructPathFromCallTargetWithFieldBindingIndex(
              exprIn,
              resolveExprPath,
              fieldIndex,
              [&](const std::string &defPath) {
                return inferStructReturnPathFromDefinitionMapByCallTargetWithFieldIndex(
                    defPath, defMap, resolveStructTypeName, resolveExprPath, fieldIndex);
              });
          if (!indexedStruct.empty()) {
            return indexedStruct;
          }
          if (visitedDefs.insert(resolvedPath).second) {
            const Definition &nestedDef = *defIt->second;
            std::string returnedStruct = inferStructReturnPathFromDefinition(
                nestedDef,
                resolveStructTypeName,
                [&](const Expr &nestedExpr) { return inferStructExprPath(nestedExpr, localsInExpr); });
            visitedDefs.erase(resolvedPath);
            if (!returnedStruct.empty()) {
              return returnedStruct;
            }
          }
        }
      }
      if (exprIn.isMethodCall && !exprIn.args.empty()) {
        const std::string receiverStruct = inferStructExprPath(exprIn.args.front(), localsInExpr);
        if (!receiverStruct.empty()) {
          const std::string methodPath = receiverStruct + "/" + exprIn.name;
          if (defMap.count(methodPath) > 0) {
            const std::string methodStruct = inferStructReturnPathFromDefinitionMapByCallTargetWithFieldIndex(
                methodPath, defMap, resolveStructTypeName, resolveExprPath, fieldIndex);
            if (!methodStruct.empty()) {
              return methodStruct;
            }
          }
        }
      }
      return inferStructPathFromCallTargetWithFieldBindingIndex(
          exprIn,
          resolveExprPath,
          fieldIndex,
          [&](const std::string &defPath) {
            return inferStructReturnPathFromDefinitionMapByCallTargetWithFieldIndex(
                defPath, defMap, resolveStructTypeName, resolveExprPath, fieldIndex);
          });
    }
    return "";
  };
  return inferStructExprPath(expr, localsIn);
}

InferStructExprWithLocalsFn makeInferStructExprPathFromDefinitionMapByCallTargetWithFieldIndex(
    const std::unordered_map<std::string, const Definition *> &defMap,
    const ResolveStructTypeNameFn &resolveStructTypeName,
    const InferStructExprPathFn &resolveExprPath,
    const UninitializedFieldBindingIndex &fieldIndex,
    const ResolveStructFieldSlotFn &resolveStructFieldSlot) {
  return [defMap, resolveStructTypeName, resolveExprPath, fieldIndex, resolveStructFieldSlot](
             const Expr &expr, const LocalMap &localsIn) {
    return inferStructExprPathFromDefinitionMapByCallTargetWithFieldIndex(
        expr, localsIn, defMap, resolveStructTypeName, resolveExprPath, fieldIndex, resolveStructFieldSlot);
  };
}

} // namespace primec::ir_lowerer
