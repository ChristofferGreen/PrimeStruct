#include "IrLowererUninitializedTypeHelpers.h"

#include <algorithm>
#include <cctype>
#include <functional>
#include <sstream>
#include <unordered_set>

#include "IrLowererHelpers.h"
#include "IrLowererBindingTransformHelpers.h"
#include "IrLowererSetupTypeCollectionHelpers.h"
#include "IrLowererStructFieldBindingHelpers.h"
#include "IrLowererTemplateTypeParseHelpers.h"
#include "primec/SoaPathHelpers.h"

namespace primec::ir_lowerer {

namespace {

bool isSpecializedExperimentalMapStructPath(const std::string &typeText) {
  std::string normalized = trimTemplateTypeText(typeText);
  if (!normalized.empty() && normalized.front() != '/') {
    normalized.insert(normalized.begin(), '/');
  }
  return normalized.rfind("/std/collections/experimental_map/Map__", 0) == 0;
}

bool isBuiltinVectorTypeName(const std::string &typeName) {
  return typeName == "vector" || typeName == "/vector" || typeName == "std/collections/vector" ||
         typeName == "/std/collections/vector";
}

std::string resolveSpecializedExperimentalVectorStructPath(
    std::string typeText) {
  typeText = trimTemplateTypeText(typeText);
  while (!typeText.empty()) {
    std::string base;
    std::string argText;
    if (!splitTemplateTypeName(typeText, base, argText)) {
      break;
    }
    base = normalizeCollectionBindingTypeName(trimTemplateTypeText(base));
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
  base = normalizeCollectionBindingTypeName(trimTemplateTypeText(base));
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

std::string resolveScopedCallPath(const Expr &expr) {
  if (expr.name.find('/') != std::string::npos || expr.namespacePrefix.empty()) {
    return expr.name;
  }
  if (expr.namespacePrefix == "/") {
    return "/" + expr.name;
  }
  return expr.namespacePrefix + "/" + expr.name;
}

const Expr *findForwardedReturnValueExpr(const Definition &definition) {
  if (definition.returnExpr.has_value()) {
    return &*definition.returnExpr;
  }
  const Expr *valueExpr = nullptr;
  for (const auto &stmt : definition.statements) {
    if (stmt.isBinding) {
      continue;
    }
    valueExpr = &stmt;
    if (isReturnCall(stmt) && stmt.args.size() == 1) {
      return &stmt.args.front();
    }
  }
  return valueExpr;
}

bool resolveForwardedReturnParameterName(const Definition &definition,
                                         std::string &parameterNameOut) {
  parameterNameOut.clear();
  const Expr *valueExpr = findForwardedReturnValueExpr(definition);
  if (valueExpr == nullptr) {
    return false;
  }
  if (isReturnCall(*valueExpr) && valueExpr->args.size() == 1) {
    valueExpr = &valueExpr->args.front();
  }
  if (valueExpr->kind != Expr::Kind::Name) {
    return false;
  }
  for (const auto &parameter : definition.parameters) {
    if (parameter.name == valueExpr->name) {
      parameterNameOut = valueExpr->name;
      return true;
    }
  }
  return false;
}

const Expr *resolveCallArgumentForParameter(const Expr &callExpr,
                                            const Definition &callee,
                                            const std::string &parameterName) {
  for (size_t index = 0; index < callExpr.args.size(); ++index) {
    if (index < callExpr.argNames.size() &&
        callExpr.argNames[index].has_value() &&
        *callExpr.argNames[index] == parameterName) {
      return &callExpr.args[index];
    }
  }
  for (size_t index = 0; index < callee.parameters.size(); ++index) {
    if (callee.parameters[index].name != parameterName) {
      continue;
    }
    return index < callExpr.args.size() ? &callExpr.args[index] : nullptr;
  }
  return nullptr;
}

bool isForwardedMapNewConstructor(const Expr &expr) {
  std::string constructorName;
  return resolvePublishedStdlibSurfaceConstructorExprMemberName(
             expr,
             primec::StdlibSurfaceId::CollectionsMapConstructors,
             constructorName) &&
         constructorName == "mapNew";
}

std::string normalizeUninitializedVectorStructPath(const std::string &typeName) {
  if (isBuiltinVectorTypeName(typeName)) {
    return "/vector";
  }
  if (typeName == "Vector") {
    return "/std/collections/experimental_vector/Vector";
  }
  if (typeName == "std/collections/experimental_vector/Vector" ||
      typeName.rfind("std/collections/experimental_vector/Vector__", 0) == 0) {
    return "/" + typeName;
  }
  return typeName;
}

std::string resolveSpecializedExperimentalSoaVectorStructPath(
    const std::string &typeText) {
  std::string normalized = trimTemplateTypeText(typeText);
  while (true) {
    if (!normalized.empty() && normalized.front() != '/') {
      normalized.insert(normalized.begin(), '/');
    }
    if (soa_paths::isExperimentalSoaVectorSpecializedTypePath(normalized)) {
      return normalized;
    }

    std::string base;
    std::string argList;
    if (!splitTemplateTypeName(normalized, base, argList)) {
      return "";
    }

    const std::string normalizedBase =
        normalizeCollectionBindingTypeName(trimTemplateTypeText(base));
    if ((normalizedBase == "Reference" || normalizedBase == "Pointer") &&
        !argList.empty()) {
      std::string unwrappedType;
      std::vector<std::string> wrappedArgs;
      if (splitTemplateArgs(argList, wrappedArgs) && wrappedArgs.size() == 1) {
        normalized = trimTemplateTypeText(wrappedArgs.front());
      } else {
        normalized = trimTemplateTypeText(argList);
      }
      if (extractTopLevelUninitializedTypeText(normalized, unwrappedType)) {
        normalized = std::move(unwrappedType);
      }
      continue;
    }

    if (normalizedBase != "soa_vector" || argList.empty()) {
      return "";
    }

    std::vector<std::string> templateArgs;
    if (!splitTemplateArgs(argList, templateArgs) || templateArgs.size() != 1) {
      return "";
    }

    std::string normalizedArg = trimTemplateTypeText(templateArgs.front());
    if (!normalizedArg.empty() && normalizedArg.front() == '/') {
      normalizedArg.erase(normalizedArg.begin());
    }
    return specializedExperimentalSoaVectorStructPathForElementType(
        normalizedArg);
  }
}

std::string inferExperimentalSoaElementStructPathFromReceiverStruct(
    const std::string &receiverStructPath,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const ResolveStructTypeNameFn &resolveStructTypeName) {
  std::string normalizedReceiverStruct = trimTemplateTypeText(receiverStructPath);
  if (!normalizedReceiverStruct.empty() &&
      normalizedReceiverStruct.front() != '/') {
    normalizedReceiverStruct.insert(normalizedReceiverStruct.begin(), '/');
  }
  if (!soa_paths::isExperimentalSoaVectorSpecializedTypePath(normalizedReceiverStruct)) {
    return "";
  }

  auto defIt = defMap.find(normalizedReceiverStruct);
  if (defIt == defMap.end() || defIt->second == nullptr) {
    return "";
  }

  LayoutFieldBinding storageBinding;
  bool foundStorageBinding = false;
  for (const auto &stmt : defIt->second->statements) {
    if (!stmt.isBinding || stmt.name != "storage") {
      continue;
    }
    if (!extractExplicitLayoutFieldBinding(stmt, storageBinding)) {
      return "";
    }
    foundStorageBinding = true;
    break;
  }
  if (!foundStorageBinding) {
    return "";
  }

  std::string columnStructPath = trimTemplateTypeText(storageBinding.typeName);
  if (!columnStructPath.empty() && columnStructPath.front() != '/') {
    columnStructPath.insert(columnStructPath.begin(), '/');
  }
  if (columnStructPath.rfind(
          "/std/collections/internal_soa_storage/SoaColumn__", 0) != 0) {
    return "";
  }

  auto columnIt = defMap.find(columnStructPath);
  if (columnIt == defMap.end() || columnIt->second == nullptr) {
    return "";
  }

  LayoutFieldBinding dataBinding;
  bool foundDataBinding = false;
  for (const auto &stmt : columnIt->second->statements) {
    if (!stmt.isBinding || stmt.name != "data") {
      continue;
    }
    if (!extractExplicitLayoutFieldBinding(stmt, dataBinding)) {
      return "";
    }
    foundDataBinding = true;
    break;
  }
  if (!foundDataBinding || dataBinding.typeTemplateArg.empty()) {
    return "";
  }

  std::string pointeeType = trimTemplateTypeText(dataBinding.typeTemplateArg);
  std::string pointeeBase;
  std::string pointeeArgs;
  if (!splitTemplateTypeName(pointeeType, pointeeBase, pointeeArgs) ||
      normalizeCollectionBindingTypeName(pointeeBase) != "uninitialized") {
    return "";
  }

  std::string resolvedStructPath;
  if (resolveStructTypeName(pointeeArgs, std::string{}, resolvedStructPath)) {
    return resolvedStructPath;
  }
  return "";
}

std::string inferUninitializedTargetStructPath(const std::string &typeText,
                                               const std::string &namespacePrefix,
                                               const ResolveStructTypeNameFn &resolveStructTypeName) {
  std::string normalized = trimTemplateTypeText(typeText);
  if (normalized.empty()) {
    return "";
  }

  const std::string specializedSoaStruct =
      resolveSpecializedExperimentalSoaVectorStructPath(normalized);
  if (!specializedSoaStruct.empty()) {
    return specializedSoaStruct;
  }

  const std::string specializedVectorStruct =
      resolveSpecializedExperimentalVectorStructPath(normalized);
  if (!specializedVectorStruct.empty()) {
    return specializedVectorStruct;
  }

  if (isSpecializedExperimentalMapStructPath(normalized)) {
    if (normalized.front() != '/') {
      normalized.insert(normalized.begin(), '/');
    }
    return normalized;
  }

  std::string resolvedStructPath;
  if (resolveStructTypeName(normalized, namespacePrefix, resolvedStructPath)) {
    return resolvedStructPath;
  }

  std::string base;
  std::string argList;
  if (!splitTemplateTypeName(normalized, base, argList)) {
    return "";
  }

  const std::string normalizedBase = normalizeCollectionBindingTypeName(base);
  if (normalizedBase == "vector") {
    return normalizeUninitializedVectorStructPath(base);
  }
  if (normalizedBase != "map") {
    return "";
  }

  std::vector<std::string> templateArgs;
  if (!splitTemplateArgs(argList, templateArgs) || templateArgs.size() != 2) {
    return "";
  }
  std::string canonicalArgs = joinTemplateArgsText(templateArgs);
  canonicalArgs.erase(
      std::remove_if(canonicalArgs.begin(), canonicalArgs.end(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
      }),
      canonicalArgs.end());

  uint64_t hash = 1469598103934665603ULL;
  for (unsigned char ch : canonicalArgs) {
    hash ^= static_cast<uint64_t>(ch);
    hash *= 1099511628211ULL;
  }

  std::ostringstream specializedPath;
  specializedPath << "/std/collections/experimental_map/Map__t" << std::hex << hash;
  return specializedPath.str();
}

} // namespace

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
      const std::string scopedCallPath = resolveScopedCallPath(exprIn);
      const auto isBareOrInternalSoaHelper = [&](std::string_view helperName) {
        const std::string helperText(helperName);
        if (isSimpleCallName(exprIn, helperText.c_str())) {
          return true;
        }
        return scopedCallPath == "/" + helperText ||
               scopedCallPath == "/std/collections/internal_soa_storage/" + helperText;
      };
      if (!exprIn.isMethodCall && !exprIn.isFieldAccess &&
          isSimpleCallName(exprIn, "pick") && exprIn.args.size() == 1 &&
          (exprIn.hasBodyArguments || !exprIn.bodyArguments.empty())) {
        const std::string sumStructPath = inferStructExprPath(exprIn.args.front(), localsInExpr);
        auto sumIt = defMap.find(sumStructPath);
        if (sumIt == defMap.end() || sumIt->second == nullptr ||
            !definitionHasTransform(*sumIt->second, "sum")) {
          return "";
        }
        const Definition &sumDef = *sumIt->second;
        auto findSumVariantByName = [&](const std::string &variantName) -> const SumVariant * {
          for (const SumVariant &candidate : sumDef.sumVariants) {
            if (candidate.name == variantName) {
              return &candidate;
            }
          }
          return nullptr;
        };
        auto isPayloadArm = [](const Expr &arm) {
          const bool hasNamedArgs = std::any_of(
              arm.argNames.begin(), arm.argNames.end(), [](const auto &argName) { return argName.has_value(); });
          return arm.kind == Expr::Kind::Call && !arm.isBinding &&
                 !arm.isMethodCall && !arm.isFieldAccess && arm.hasBodyArguments &&
                 arm.templateArgs.empty() && !hasNamedArgs && arm.args.size() == 1 &&
                 arm.args.front().kind == Expr::Kind::Name;
        };
        auto isUnitCallArm = [](const Expr &arm) {
          const bool hasNamedArgs = std::any_of(
              arm.argNames.begin(), arm.argNames.end(), [](const auto &argName) { return argName.has_value(); });
          return arm.kind == Expr::Kind::Call && !arm.isBinding &&
                 !arm.isMethodCall && !arm.isFieldAccess && arm.hasBodyArguments &&
                 arm.templateArgs.empty() && !hasNamedArgs && arm.args.empty();
        };
        auto isUnitBindingArm = [](const Expr &arm) {
          return arm.kind == Expr::Kind::Call && arm.isBinding &&
                 !arm.isMethodCall && !arm.isFieldAccess && !arm.name.empty() &&
                 arm.transforms.empty() && arm.templateArgs.empty() &&
                 !arm.hasBodyArguments && arm.bodyArguments.empty() &&
                 arm.args.size() == 1 && arm.argNames.size() == 1 &&
                 !arm.argNames.front().has_value();
        };
        auto armBodyExprs = [&](const Expr &arm) {
          std::vector<const Expr *> out;
          if (isPayloadArm(arm) || isUnitCallArm(arm)) {
            out.reserve(arm.bodyArguments.size());
            for (const Expr &bodyExpr : arm.bodyArguments) {
              out.push_back(&bodyExpr);
            }
            return out;
          }
          if (!isUnitBindingArm(arm)) {
            return out;
          }
          const Expr &initializer = arm.args.front();
          if (initializer.kind == Expr::Kind::Call && initializer.name == "block" &&
              initializer.hasBodyArguments && initializer.args.empty()) {
            out.reserve(initializer.bodyArguments.size());
            for (const Expr &bodyExpr : initializer.bodyArguments) {
              out.push_back(&bodyExpr);
            }
            return out;
          }
          out.push_back(&initializer);
          return out;
        };
        std::string resultStructPath;
        bool sawAggregateResult = false;
        for (const Expr &arm : exprIn.bodyArguments) {
          const bool payloadArm = isPayloadArm(arm);
          const bool unitArm = isUnitCallArm(arm) || isUnitBindingArm(arm);
          if (!payloadArm && !unitArm) {
            return "";
          }
          const SumVariant *variant = findSumVariantByName(arm.name);
          if (variant == nullptr || variant->hasPayload != payloadArm) {
            return "";
          }
          LocalMap branchLocals = localsInExpr;
          if (payloadArm) {
            std::string payloadType = !variant->payloadTypeText.empty()
                                          ? trimTemplateTypeText(variant->payloadTypeText)
                                          : trimTemplateTypeText(variant->payloadType);
            if (variant->payloadTypeText.empty() && !variant->payloadTemplateArgs.empty()) {
              payloadType += "<" + joinTemplateArgsText(variant->payloadTemplateArgs) + ">";
            }
            std::string payloadStructPath;
            resolveStructTypeName(payloadType, sumDef.namespacePrefix, payloadStructPath);
            LocalInfo payloadInfo;
            payloadInfo.kind = LocalInfo::Kind::Value;
            payloadInfo.valueKind =
                payloadStructPath.empty() ? LocalInfo::ValueKind::Unknown : LocalInfo::ValueKind::Int64;
            payloadInfo.structTypeName = payloadStructPath;
            branchLocals.emplace(arm.args.front().name, payloadInfo);
          }
          const Expr *valueExpr = nullptr;
          bool sawReturn = false;
          for (const Expr *bodyExprPtr : armBodyExprs(arm)) {
            const Expr &bodyExpr = *bodyExprPtr;
            if (bodyExpr.isBinding) {
              continue;
            }
            if (isSimpleCallName(bodyExpr, "return")) {
              if (bodyExpr.args.size() != 1) {
                return "";
              }
              valueExpr = &bodyExpr.args.front();
              sawReturn = true;
              continue;
            }
            if (!sawReturn) {
              valueExpr = &bodyExpr;
            }
          }
          if (valueExpr == nullptr) {
            return "";
          }
          const std::string branchStructPath = inferStructExprPath(*valueExpr, branchLocals);
          if (branchStructPath.empty()) {
            return "";
          }
          if (!sawAggregateResult) {
            resultStructPath = branchStructPath;
            sawAggregateResult = true;
            continue;
          }
          if (branchStructPath != resultStructPath) {
            return "";
          }
        }
        return sawAggregateResult ? resultStructPath : std::string{};
      }
      if (!exprIn.isMethodCall && exprIn.args.empty() && exprIn.templateArgs.empty() &&
          exprIn.bodyArguments.empty()) {
        Expr syntheticNameExpr;
        syntheticNameExpr.kind = Expr::Kind::Name;
        syntheticNameExpr.name = exprIn.name;
        syntheticNameExpr.namespacePrefix = exprIn.namespacePrefix;
        const std::string localStructPath = inferStructPathFromNameExpr(syntheticNameExpr, localsInExpr);
        if (!localStructPath.empty()) {
          return localStructPath;
        }
      }
      if (!exprIn.isMethodCall && exprIn.name == "uninitialized" && exprIn.args.empty() &&
          exprIn.templateArgs.size() == 1) {
        return inferUninitializedTargetStructPath(
            exprIn.templateArgs.front(), exprIn.namespacePrefix, resolveStructTypeName);
      }
      if (!exprIn.isMethodCall && !exprIn.isFieldAccess && exprIn.isBraceConstructor &&
          !exprIn.name.empty()) {
        std::string constructorType = trimTemplateTypeText(exprIn.name);
        if (!exprIn.templateArgs.empty()) {
          constructorType += "<" + joinTemplateArgsText(exprIn.templateArgs) + ">";
        }
        const std::string constructorStruct = inferUninitializedTargetStructPath(
            constructorType, exprIn.namespacePrefix, resolveStructTypeName);
        if (!constructorStruct.empty()) {
          return constructorStruct;
        }
      }
      if (!exprIn.isMethodCall && isBareOrInternalSoaHelper("location") && exprIn.args.size() == 1) {
        return inferStructExprPath(exprIn.args.front(), localsInExpr);
      }
      if (!exprIn.isMethodCall && isBareOrInternalSoaHelper("dereference") && exprIn.args.size() == 1) {
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
      if (!exprIn.isMethodCall && exprIn.templateArgs.size() == 2) {
        std::string collectionName;
        if (getBuiltinCollectionName(exprIn, collectionName) && collectionName == "map") {
          const std::string mapType = "map<" + trimTemplateTypeText(exprIn.templateArgs.front()) + ", " +
                                      trimTemplateTypeText(exprIn.templateArgs.back()) + ">";
          const std::string inferredMapStruct =
              inferUninitializedTargetStructPath(mapType, exprIn.namespacePrefix, resolveStructTypeName);
          if (!inferredMapStruct.empty()) {
            return inferredMapStruct;
          }
        }
      }
      const std::string canonicalSoaGetPath =
          soa_paths::canonicalizeLegacySoaGetHelperPath(scopedCallPath);
      const std::string canonicalSoaRefPath =
          soa_paths::canonicalizeLegacySoaRefHelperPath(scopedCallPath);
      const bool isSoaGetLikeHelper =
          soa_paths::isLegacyOrCanonicalSoaHelperPath(canonicalSoaGetPath, "get") ||
          soa_paths::isLegacyOrCanonicalSoaHelperPath(canonicalSoaGetPath, "get_ref");
      const bool isSoaRefLikeHelper =
          soa_paths::isLegacyOrCanonicalSoaHelperPath(canonicalSoaRefPath, "ref") ||
          soa_paths::isLegacyOrCanonicalSoaHelperPath(canonicalSoaRefPath, "ref_ref");
      if ((isBareOrInternalSoaHelper("get") || isBareOrInternalSoaHelper("ref") ||
           isSoaGetLikeHelper || isSoaRefLikeHelper) &&
          exprIn.args.size() == 2) {
        const std::string receiverStruct = inferStructExprPath(exprIn.args.front(),
                                                               localsInExpr);
        const std::string elementStruct =
            inferExperimentalSoaElementStructPathFromReceiverStruct(
                receiverStruct, defMap, resolveStructTypeName);
        if (!elementStruct.empty()) {
          return elementStruct;
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
      const bool isExplicitMapArgsPackAt =
          !exprIn.isMethodCall &&
          (scopedCallPath == "/map/at" || scopedCallPath == "/std/collections/map/at") &&
          exprIn.args.size() == 2;
      if ((getBuiltinArrayAccessName(exprIn, accessName) && exprIn.args.size() == 2) ||
          isExplicitMapArgsPackAt) {
        const Expr &accessReceiver = exprIn.args.front();
        if (accessReceiver.kind == Expr::Kind::Name) {
          auto receiverIt = localsInExpr.find(accessReceiver.name);
          if (receiverIt != localsInExpr.end() && receiverIt->second.isArgsPack &&
              !receiverIt->second.isFileHandle &&
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
        const std::string rawCallPath = resolveScopedCallPath(exprIn);
        const std::string slashPrefixedRawCallPath =
            (!rawCallPath.empty() && rawCallPath.front() != '/') ? "/" + rawCallPath : rawCallPath;
        auto inferPublishedMapConstructorStruct = [&](const std::string &candidatePath) {
          const std::string publishedMapStruct =
              inferPublishedExperimentalMapStructPathFromConstructorPath(candidatePath);
          if (!publishedMapStruct.empty()) {
            return publishedMapStruct;
          }
          return std::string{};
        };
        if (exprIn.templateArgs.size() == 2 &&
            isPublishedStdlibSurfaceConstructorExpr(
                exprIn,
                primec::StdlibSurfaceId::CollectionsMapConstructors)) {
          const std::string mapType =
              "map<" + trimTemplateTypeText(exprIn.templateArgs.front()) + ", " +
              trimTemplateTypeText(exprIn.templateArgs.back()) + ">";
          const std::string templateStruct =
              inferUninitializedTargetStructPath(mapType, exprIn.namespacePrefix, resolveStructTypeName);
          if (!templateStruct.empty()) {
            return templateStruct;
          }
        }
        if (const std::string publishedStruct = inferPublishedMapConstructorStruct(resolvedPath);
            !publishedStruct.empty()) {
          return publishedStruct;
        }
        if (rawCallPath != resolvedPath) {
          if (const std::string publishedStruct = inferPublishedMapConstructorStruct(rawCallPath);
              !publishedStruct.empty()) {
            return publishedStruct;
          }
        }
        if (slashPrefixedRawCallPath != resolvedPath &&
            slashPrefixedRawCallPath != rawCallPath) {
          if (const std::string publishedStruct =
                  inferPublishedMapConstructorStruct(slashPrefixedRawCallPath);
              !publishedStruct.empty()) {
            return publishedStruct;
          }
        }
        auto defIt = defMap.find(resolvedPath);
        if (defIt == defMap.end() && rawCallPath != resolvedPath) {
          defIt = defMap.find(rawCallPath);
        }
        if (defIt == defMap.end() && slashPrefixedRawCallPath != resolvedPath &&
            slashPrefixedRawCallPath != rawCallPath) {
          defIt = defMap.find(slashPrefixedRawCallPath);
        }
        if (defIt != defMap.end() && defIt->second != nullptr) {
          const std::string definitionPath = defIt->second->fullPath;
          if (isStructDefinition(*defIt->second)) {
            return definitionPath;
          }
          auto inferExplicitStructReturnPath = [&](const Definition &def) -> std::string {
            for (const auto &transform : def.transforms) {
              if (transform.name != "return" || transform.templateArgs.size() != 1) {
                continue;
              }
              std::string candidateType = transform.templateArgs.front();
              std::string base;
              std::string argText;
              if (splitTemplateTypeName(candidateType, base, argText) &&
                  (base == "Reference" || base == "Pointer")) {
                std::vector<std::string> args;
                if (splitTemplateArgs(argText, args) && args.size() == 1) {
                  candidateType = args.front();
                } else {
                  candidateType = argText;
                }
              }
              if (candidateType.empty()) {
                continue;
              }
              std::string inferredReturnStruct = inferUninitializedTargetStructPath(
                  candidateType, exprIn.namespacePrefix, resolveStructTypeName);
              if (!inferredReturnStruct.empty()) {
                return inferredReturnStruct;
              }
              if (candidateType.front() == '/') {
                return candidateType;
              }
            }
            return "";
          };
          const std::string explicitReturnStruct =
              inferExplicitStructReturnPath(*defIt->second);
          if (!explicitReturnStruct.empty()) {
            return explicitReturnStruct;
          }
          auto inferForwardedCallArgStruct = [&](const Definition &def) -> std::string {
            std::string forwardedParamName;
            if (!resolveForwardedReturnParameterName(def, forwardedParamName)) {
              return "";
            }
            const Expr *forwardedArg =
                resolveCallArgumentForParameter(exprIn, def, forwardedParamName);
            if (forwardedArg == nullptr ||
                isForwardedMapNewConstructor(*forwardedArg)) {
              return "";
            }
            return inferStructExprPath(*forwardedArg, localsInExpr);
          };
          if (const std::string forwardedStruct =
                  inferForwardedCallArgStruct(*defIt->second);
              !forwardedStruct.empty()) {
            return forwardedStruct;
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
          if (visitedDefs.insert(definitionPath).second) {
            const Definition &nestedDef = *defIt->second;
            std::string returnedStruct = inferStructReturnPathFromDefinition(
                nestedDef,
                resolveStructTypeName,
                [&](const Expr &nestedExpr) { return inferStructExprPath(nestedExpr, localsInExpr); });
            visitedDefs.erase(definitionPath);
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
