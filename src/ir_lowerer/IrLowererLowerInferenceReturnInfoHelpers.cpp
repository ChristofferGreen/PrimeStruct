#include "IrLowererLowerInferenceSetup.h"

#include "IrLowererResultHelpers.h"
#include "IrLowererSemanticProductTargetAdapters.h"
#include "IrLowererSetupTypeHelpers.h"
#include "IrLowererTemplateTypeParseHelpers.h"
#include "primec/SemanticReturnDependencyOrder.h"

#include <algorithm>
#include <limits>
#include <vector>

namespace primec::ir_lowerer {

namespace {

bool returnInfoEquals(const ReturnInfo &left, const ReturnInfo &right) {
  return left.returnsVoid == right.returnsVoid && left.returnsArray == right.returnsArray &&
         left.kind == right.kind && left.isResult == right.isResult &&
         left.resultHasValue == right.resultHasValue &&
         left.resultValueCollectionKind == right.resultValueCollectionKind &&
         left.resultValueMapKeyKind == right.resultValueMapKeyKind &&
         left.resultValueIsFileHandle == right.resultValueIsFileHandle &&
         left.resultValueStructType == right.resultValueStructType &&
         left.resultValueKind == right.resultValueKind &&
         left.resultErrorType == right.resultErrorType;
}

bool isSumDefinitionForReturnInfo(const Definition &definition) {
  for (const auto &transform : definition.transforms) {
    if (transform.name == "sum") {
      return true;
    }
  }
  return false;
}

bool isSumDefinitionForReturnInfo(const Definition &definition,
                                  const SemanticProgram *semanticProgram) {
  if (isSumDefinitionForReturnInfo(definition)) {
    return true;
  }
  if (semanticProgram == nullptr) {
    return false;
  }
  if (const auto *typeMetadata =
          semanticProgramLookupTypeMetadata(*semanticProgram, definition.fullPath);
      typeMetadata != nullptr && typeMetadata->category == "sum") {
    return true;
  }
  return semanticProgramLookupPublishedSumTypeMetadata(*semanticProgram, definition.fullPath) != nullptr;
}

bool assignTypeDefinitionReturnInfo(const Definition &definition,
                                    const SemanticProgram *semanticProgram,
                                    ReturnInfo &infoOut) {
  if (!isSumDefinitionForReturnInfo(definition, semanticProgram)) {
    return false;
  }
  infoOut = {};
  infoOut.returnsArray = true;
  return true;
}

void sortDefinitionsForDeterministicReturnSolve(std::vector<const Definition *> &definitions) {
  std::stable_sort(definitions.begin(), definitions.end(), [](const Definition *left, const Definition *right) {
    const int leftLine = left->sourceLine > 0 ? left->sourceLine : std::numeric_limits<int>::max();
    const int rightLine = right->sourceLine > 0 ? right->sourceLine : std::numeric_limits<int>::max();
    if (leftLine != rightLine) {
      return leftLine < rightLine;
    }
    const int leftColumn = left->sourceColumn > 0 ? left->sourceColumn : std::numeric_limits<int>::max();
    const int rightColumn = right->sourceColumn > 0 ? right->sourceColumn : std::numeric_limits<int>::max();
    if (leftColumn != rightColumn) {
      return leftColumn < rightColumn;
    }
    return left->fullPath < right->fullPath;
  });
}

std::vector<const Definition *> collectReturnInfoComponentDefinitions(
    const semantics::ReturnDependencyComponent &component,
    const std::unordered_map<std::string, const Definition *> &defMap) {
  std::vector<const Definition *> definitions;
  definitions.reserve(component.definitionPaths.size());
  for (const std::string &definitionPath : component.definitionPaths) {
    const auto defIt = defMap.find(definitionPath);
    if (defIt == defMap.end() || defIt->second == nullptr) {
      continue;
    }
    definitions.push_back(defIt->second);
  }
  sortDefinitionsForDeterministicReturnSolve(definitions);
  return definitions;
}

bool isSemanticFileHandleTypeText(const std::string &typeText) {
  std::string base;
  std::string args;
  return splitTemplateTypeName(trimTemplateTypeText(typeText), base, args) &&
         normalizeCollectionBindingTypeName(base) == "File";
}

std::string resolveSemanticProductReturnText(const SemanticProgram &semanticProgram,
                                             const std::string &text,
                                             SymbolId textId) {
  if (textId != InvalidSymbolId) {
    std::string resolvedText =
        std::string(semanticProgramResolveCallTargetString(semanticProgram, textId));
    if (!resolvedText.empty()) {
      return trimTemplateTypeText(resolvedText);
    }
  }
  return trimTemplateTypeText(text);
}

void clearReturnInfoResultValue(ReturnInfo &info) {
  info.resultHasValue = false;
  info.resultValueKind = LocalInfo::ValueKind::Unknown;
  info.resultValueCollectionKind = LocalInfo::Kind::Value;
  info.resultValueMapKeyKind = LocalInfo::ValueKind::Unknown;
  info.resultValueIsFileHandle = false;
  info.resultValueStructType.clear();
  info.resultErrorType.clear();
}

bool applySemanticResultValueTypeText(const LowerInferenceReturnInfoSetupInput &input,
                                      const Definition &definition,
                                      const std::string &valueTypeText,
                                      ReturnInfo &infoOut) {
  const std::string trimmedValueType = trimTemplateTypeText(valueTypeText);
  if (trimmedValueType.empty()) {
    return false;
  }

  infoOut.resultValueCollectionKind = LocalInfo::Kind::Value;
  infoOut.resultValueKind = LocalInfo::ValueKind::Unknown;
  infoOut.resultValueMapKeyKind = LocalInfo::ValueKind::Unknown;
  infoOut.resultValueIsFileHandle = false;
  infoOut.resultValueStructType.clear();

  if (resolveSupportedResultCollectionType(
          trimmedValueType,
          infoOut.resultValueCollectionKind,
          infoOut.resultValueKind,
          &infoOut.resultValueMapKeyKind)) {
    return true;
  }
  if (isSemanticFileHandleTypeText(trimmedValueType)) {
    infoOut.resultValueKind = LocalInfo::ValueKind::Int64;
    infoOut.resultValueIsFileHandle = true;
    return true;
  }

  infoOut.resultValueKind = valueKindFromTypeName(trimmedValueType);
  if (infoOut.resultValueKind != LocalInfo::ValueKind::Unknown) {
    return true;
  }

  if (input.resolveStructTypeName) {
    std::string structPath;
    if (input.resolveStructTypeName(trimmedValueType, definition.namespacePrefix, structPath)) {
      infoOut.resultValueStructType = std::move(structPath);
      return true;
    }
  }

  infoOut.resultValueStructType = trimmedValueType;
  return true;
}

bool buildSemanticProductReturnInfo(const LowerInferenceReturnInfoSetupInput &input,
                                    const SemanticProgram &semanticProgram,
                                    const SemanticProductIndex &semanticIndex,
                                    const Definition &definition,
                                    ReturnInfo &infoOut,
                                    std::string &errorOut) {
  if (assignTypeDefinitionReturnInfo(definition, &semanticProgram, infoOut)) {
    return true;
  }
  if (!input.resolveStructTypeName) {
    errorOut = "native backend missing inference return-info setup dependency: resolveStructTypeName";
    return false;
  }
  if (!input.resolveStructArrayInfoFromPath) {
    errorOut = "native backend missing inference return-info setup dependency: resolveStructArrayInfoFromPath";
    return false;
  }

  const auto *callableSummary =
      findSemanticProductCallableSummary(&semanticProgram, definition.fullPath);
  if (callableSummary == nullptr) {
    errorOut = "missing semantic-product callable summary: " + definition.fullPath;
    return false;
  }

  const auto *returnFact = findSemanticProductReturnFact(&semanticProgram, semanticIndex, definition);
  if (returnFact == nullptr) {
    errorOut = "missing semantic-product return fact: " + definition.fullPath;
    return false;
  }
  const std::string returnBindingTypeText = resolveSemanticProductReturnText(
      semanticProgram, returnFact->bindingTypeText, returnFact->bindingTypeTextId);
  if (returnBindingTypeText.empty()) {
    errorOut = "missing semantic-product return binding type: " + definition.fullPath;
    return false;
  }

  Definition semanticReturnDefinition;
  semanticReturnDefinition.fullPath = definition.fullPath;
  semanticReturnDefinition.namespacePrefix = definition.namespacePrefix;
  semanticReturnDefinition.sourceLine = definition.sourceLine;
  semanticReturnDefinition.sourceColumn = definition.sourceColumn;
  semanticReturnDefinition.semanticNodeId = definition.semanticNodeId;
  semanticReturnDefinition.transforms.push_back(Transform{
      .name = "return",
      .templateArgs = {returnBindingTypeText},
  });

  infoOut = {};
  bool hasReturnTransform = false;
  bool hasReturnAuto = false;
  analyzeDeclaredReturnTransforms(
      semanticReturnDefinition,
      [&](const std::string &typeName, const std::string &namespacePrefix, std::string &structPathOut) {
        return input.resolveStructTypeName(typeName, namespacePrefix, structPathOut);
      },
      [&](const std::string &structPath, StructArrayTypeInfo &structInfoOut) {
        return input.resolveStructArrayInfoFromPath(structPath, structInfoOut);
      },
      infoOut,
      hasReturnTransform,
      hasReturnAuto);

  if (!hasReturnTransform || hasReturnAuto) {
    errorOut = "missing semantic-product return binding type: " + definition.fullPath;
    return false;
  }

  if (callableSummary->hasResultType) {
    const std::string resultValueType = resolveSemanticProductReturnText(
        semanticProgram, callableSummary->resultValueType, callableSummary->resultValueTypeId);
    const std::string resultErrorType = resolveSemanticProductReturnText(
        semanticProgram, callableSummary->resultErrorType, callableSummary->resultErrorTypeId);
    infoOut.returnsVoid = false;
    infoOut.returnsArray = false;
    infoOut.isResult = true;
    infoOut.resultHasValue = callableSummary->resultTypeHasValue;
    infoOut.resultErrorType = resultErrorType;
    infoOut.kind = callableSummary->resultTypeHasValue ? LocalInfo::ValueKind::Int64 : LocalInfo::ValueKind::Int32;
    infoOut.resultValueCollectionKind = LocalInfo::Kind::Value;
    infoOut.resultValueKind = LocalInfo::ValueKind::Unknown;
    infoOut.resultValueMapKeyKind = LocalInfo::ValueKind::Unknown;
    infoOut.resultValueIsFileHandle = false;
    infoOut.resultValueStructType.clear();
    if (callableSummary->resultTypeHasValue) {
      if (!applySemanticResultValueTypeText(input, definition, resultValueType, infoOut)) {
        errorOut = "missing semantic-product callable result metadata: " + definition.fullPath;
        return false;
      }
    }
  } else if (infoOut.isResult) {
    errorOut = "missing semantic-product callable result metadata: " + definition.fullPath;
    return false;
  } else {
    clearReturnInfoResultValue(infoOut);
    const std::string returnKind = resolveSemanticProductReturnText(
        semanticProgram, callableSummary->returnKind, callableSummary->returnKindId);
    if (returnKind == "void") {
      infoOut = {};
      infoOut.returnsVoid = true;
    }
  }

  if (!infoOut.returnsVoid && !infoOut.isResult && infoOut.kind == LocalInfo::ValueKind::Unknown) {
    errorOut = "native backend does not support return type on " + definition.fullPath;
    return false;
  }
  if (infoOut.returnsArray && infoOut.kind == LocalInfo::ValueKind::String) {
    errorOut = "native backend does not support string array return types on " + definition.fullPath;
    return false;
  }
  return true;
}

bool precomputeSemanticProductReturnInfoCache(const LowerInferenceGetReturnInfoSetupInput &input,
                                              const LowerInferenceReturnInfoSetupInput &returnInfoSetupInput,
                                              std::string &errorOut) {
  if (input.defMap == nullptr) {
    errorOut = "native backend missing inference get-return-info setup dependency: defMap";
    return false;
  }
  if (input.returnInfoCache == nullptr) {
    errorOut = "native backend missing inference get-return-info setup dependency: returnInfoCache";
    return false;
  }
  if (input.semanticProgram == nullptr || input.semanticIndex == nullptr) {
    errorOut = "native backend missing inference get-return-info setup dependency: semanticProgram";
    return false;
  }

  auto &returnInfoCache = *input.returnInfoCache;
  returnInfoCache.clear();

  const auto callableSummaries =
      semanticProgramCallableSummaryView(*input.semanticProgram);
  for (const auto *entry : callableSummaries) {
    if (entry == nullptr) {
      continue;
    }
    const std::string_view callablePath =
        semanticProgramCallableSummaryFullPath(*input.semanticProgram, *entry);
    if (entry->fullPathId == InvalidSymbolId || callablePath.empty()) {
      errorOut = "missing semantic-product callable summary path id";
      return false;
    }
  }

  std::vector<const Definition *> definitions;
  const auto &publishedCallableSummaryIndices =
      input.semanticProgram->publishedRoutingLookups.callableSummaryIndicesByPathId;
  definitions.reserve(!publishedCallableSummaryIndices.empty() ? publishedCallableSummaryIndices.size()
                                                               : callableSummaries.size());

  if (!publishedCallableSummaryIndices.empty()) {
    for (const auto &[pathId, callableSummaryIndex] : publishedCallableSummaryIndices) {
      if (callableSummaryIndex >= input.semanticProgram->callableSummaries.size()) {
        errorOut = "missing semantic-product callable summary path id";
        return false;
      }
      const std::string_view pathView = semanticProgramResolveCallTargetString(
          *input.semanticProgram, pathId);
      if (pathView.empty()) {
        errorOut = "missing semantic-product callable summary path id";
        return false;
      }
      const auto defIt = input.defMap->find(std::string(pathView));
      if (defIt == input.defMap->end() || defIt->second == nullptr) {
        errorOut = "native backend cannot resolve definition: " + std::string(pathView);
        return false;
      }
      definitions.push_back(defIt->second);
    }
  } else {
    for (const auto *entry : callableSummaries) {
      if (entry == nullptr || entry->fullPathId == InvalidSymbolId) {
        errorOut = "missing semantic-product callable summary path id";
        return false;
      }
      const std::string_view pathView = semanticProgramResolveCallTargetString(
          *input.semanticProgram, entry->fullPathId);
      if (pathView.empty()) {
        errorOut = "missing semantic-product callable summary path id";
        return false;
      }
      const auto defIt = input.defMap->find(std::string(pathView));
      if (defIt == input.defMap->end() || defIt->second == nullptr) {
        errorOut = "native backend cannot resolve definition: " + std::string(pathView);
        return false;
      }
      definitions.push_back(defIt->second);
    }
  }
  sortDefinitionsForDeterministicReturnSolve(definitions);

  for (const Definition *definition : definitions) {
    ReturnInfo info;
    if (!buildSemanticProductReturnInfo(
            returnInfoSetupInput,
            *input.semanticProgram,
            *input.semanticIndex,
            *definition,
            info,
            errorOut)) {
      return false;
    }
    returnInfoCache.insert_or_assign(definition->fullPath, std::move(info));
  }

  return true;
}

bool runLowerInferenceReturnInfoSetupImpl(const LowerInferenceReturnInfoSetupInput &input,
                                          const Definition &definition,
                                          ReturnInfo &infoInOut,
                                          bool deferUnknownReturnDependencyErrors,
                                          bool *sawUnresolvedDependencyOut,
                                          std::string &errorOut) {
  if (!input.resolveStructTypeName) {
    errorOut = "native backend missing inference return-info setup dependency: resolveStructTypeName";
    return false;
  }
  if (!input.resolveStructArrayInfoFromPath) {
    errorOut = "native backend missing inference return-info setup dependency: resolveStructArrayInfoFromPath";
    return false;
  }
  if (!input.isBindingMutable) {
    errorOut = "native backend missing inference return-info setup dependency: isBindingMutable";
    return false;
  }
  if (!input.bindingKind) {
    errorOut = "native backend missing inference return-info setup dependency: bindingKind";
    return false;
  }
  if (!input.hasExplicitBindingTypeTransform) {
    errorOut = "native backend missing inference return-info setup dependency: hasExplicitBindingTypeTransform";
    return false;
  }
  if (!input.bindingValueKind) {
    errorOut = "native backend missing inference return-info setup dependency: bindingValueKind";
    return false;
  }
  if (!input.inferExprKind) {
    errorOut = "native backend missing inference return-info setup dependency: inferExprKind";
    return false;
  }
  if (!input.isFileErrorBinding) {
    errorOut = "native backend missing inference return-info setup dependency: isFileErrorBinding";
    return false;
  }
  if (!input.setReferenceArrayInfo) {
    errorOut = "native backend missing inference return-info setup dependency: setReferenceArrayInfo";
    return false;
  }
  if (!input.applyStructArrayInfo) {
    errorOut = "native backend missing inference return-info setup dependency: applyStructArrayInfo";
    return false;
  }
  if (!input.applyStructValueInfo) {
    errorOut = "native backend missing inference return-info setup dependency: applyStructValueInfo";
    return false;
  }
  if (!input.inferStructExprPath) {
    errorOut = "native backend missing inference return-info setup dependency: inferStructExprPath";
    return false;
  }
  if (!input.isStringBinding) {
    errorOut = "native backend missing inference return-info setup dependency: isStringBinding";
    return false;
  }
  if (!input.inferArrayElementKind) {
    errorOut = "native backend missing inference return-info setup dependency: inferArrayElementKind";
    return false;
  }
  if (!input.lowerMatchToIf) {
    errorOut = "native backend missing inference return-info setup dependency: lowerMatchToIf";
    return false;
  }

  const auto resolveStructTypeName = input.resolveStructTypeName;
  const auto resolveStructArrayInfoFromPath = input.resolveStructArrayInfoFromPath;
  const auto isBindingMutable = input.isBindingMutable;
  const auto bindingKind = input.bindingKind;
  const auto hasExplicitBindingTypeTransform = input.hasExplicitBindingTypeTransform;
  const auto bindingValueKind = input.bindingValueKind;
  const auto inferExprKind = input.inferExprKind;
  const auto isFileErrorBinding = input.isFileErrorBinding;
  const auto setReferenceArrayInfo = input.setReferenceArrayInfo;
  const auto applyStructArrayInfo = input.applyStructArrayInfo;
  const auto applyStructValueInfo = input.applyStructValueInfo;
  const auto inferStructExprPath = input.inferStructExprPath;
  const auto isStringBinding = input.isStringBinding;
  const auto inferArrayElementKind = input.inferArrayElementKind;
  const auto lowerMatchToIf = input.lowerMatchToIf;

  bool hasReturnTransformLocal = false;
  bool hasReturnAuto = false;
  ir_lowerer::analyzeDeclaredReturnTransforms(definition,
                                              [&](const std::string &typeName,
                                                  const std::string &namespacePrefix,
                                                  std::string &structPathOut) {
                                                return resolveStructTypeName(typeName, namespacePrefix, structPathOut);
                                              },
                                              [&](const std::string &structPath, StructArrayTypeInfo &structInfoOut) {
                                                return resolveStructArrayInfoFromPath(structPath, structInfoOut);
                                              },
                                              infoInOut,
                                              hasReturnTransformLocal,
                                              hasReturnAuto);

  auto inferBindingIntoLocals = [&](const Expr &bindingExpr,
                                    bool isParameter,
                                    LocalMap &activeLocals,
                                    std::string &inferError) -> bool {
    return ir_lowerer::inferReturnInferenceBindingIntoLocals(bindingExpr,
                                                             isParameter,
                                                             definition.fullPath,
                                                             activeLocals,
                                                             isBindingMutable,
                                                             bindingKind,
                                                             hasExplicitBindingTypeTransform,
                                                             bindingValueKind,
                                                             inferExprKind,
                                                             isFileErrorBinding,
                                                             setReferenceArrayInfo,
                                                             applyStructArrayInfo,
                                                             applyStructValueInfo,
                                                             inferStructExprPath,
                                                             isStringBinding,
                                                             inferError);
  };

  auto hasDeclaredStructReturn = [&]() -> bool {
    for (const auto &transform : definition.transforms) {
      if (transform.name != "return" || transform.templateArgs.size() != 1) {
        continue;
      }
      const std::string &declaredType = transform.templateArgs.front();
      if (declaredType == "auto" || declaredType == "void") {
        return false;
      }
      std::string structPath;
      return resolveStructTypeName(declaredType, definition.namespacePrefix, structPath);
    }
    return false;
  };

  if (hasReturnTransformLocal) {
    if (hasReturnAuto) {
      std::string collectionName;
      std::vector<std::string> collectionArgs;
      if (ir_lowerer::inferDeclaredReturnCollection(definition, collectionName, collectionArgs)) {
        auto assignCollectionReturnInfo = [&](LocalInfo::ValueKind elementKind) {
          if (elementKind == LocalInfo::ValueKind::Unknown) {
            return false;
          }
          infoInOut.returnsVoid = false;
          infoInOut.returnsArray = true;
          infoInOut.kind = elementKind;
          return true;
        };
        if ((collectionName == "array" || collectionName == "vector" || collectionName == "soa_vector") &&
            collectionArgs.size() == 1 && assignCollectionReturnInfo(valueKindFromTypeName(collectionArgs.front()))) {
          if (infoInOut.kind == LocalInfo::ValueKind::String) {
            errorOut = "native backend does not support string array return types on " + definition.fullPath;
            return false;
          }
          return true;
        }
        if (collectionName == "map" && collectionArgs.size() == 2 &&
            assignCollectionReturnInfo(valueKindFromTypeName(collectionArgs.back()))) {
          return true;
        }
      }
      ReturnInferenceOptions options;
      options.missingReturnBehavior = MissingReturnBehavior::Error;
      options.includeDefinitionReturnExpr = true;
      options.deferUnknownReturnDependencyErrors = deferUnknownReturnDependencyErrors;
      if (!ir_lowerer::inferDefinitionReturnType(
              definition,
              LocalMap{},
              inferBindingIntoLocals,
              [&](const Expr &expr, const LocalMap &localsIn) { return inferExprKind(expr, localsIn); },
              [&](const Expr &expr, const LocalMap &localsIn) { return inferArrayElementKind(expr, localsIn); },
              [&](const Expr &expr, const LocalMap &localsIn) { return inferStructExprPath(expr, localsIn); },
              [&](const Expr &expr, Expr &expanded, std::string &inferError) {
                return lowerMatchToIf(expr, expanded, inferError);
              },
              options,
              infoInOut,
              errorOut,
              sawUnresolvedDependencyOut)) {
        return false;
      }
    } else if (!infoInOut.returnsVoid) {
      if (infoInOut.kind == LocalInfo::ValueKind::Unknown) {
        if (hasDeclaredStructReturn()) {
          // Struct-return helpers lower through array-handle return paths.
          infoInOut.returnsArray = true;
          infoInOut.kind = LocalInfo::ValueKind::Int32;
        } else {
          errorOut = "native backend does not support return type on " + definition.fullPath;
          return false;
        }
      }
      if (infoInOut.returnsArray && infoInOut.kind == LocalInfo::ValueKind::String) {
        errorOut = "native backend does not support string array return types on " + definition.fullPath;
        return false;
      }
    }
  } else if (!definition.hasReturnStatement) {
    infoInOut.returnsVoid = true;
  } else {
    ReturnInferenceOptions options;
    options.missingReturnBehavior = MissingReturnBehavior::Void;
    options.includeDefinitionReturnExpr = false;
    options.deferUnknownReturnDependencyErrors = deferUnknownReturnDependencyErrors;
    if (!ir_lowerer::inferDefinitionReturnType(
            definition,
            LocalMap{},
            inferBindingIntoLocals,
            [&](const Expr &expr, const LocalMap &localsIn) { return inferExprKind(expr, localsIn); },
            [&](const Expr &expr, const LocalMap &localsIn) { return inferArrayElementKind(expr, localsIn); },
            [&](const Expr &expr, const LocalMap &localsIn) { return inferStructExprPath(expr, localsIn); },
            [&](const Expr &expr, Expr &expanded, std::string &inferError) {
              return lowerMatchToIf(expr, expanded, inferError);
            },
            options,
            infoInOut,
            errorOut,
            sawUnresolvedDependencyOut)) {
      return false;
    }
  }

  return true;
}

bool precomputeOrderedReturnInfoCache(const LowerInferenceGetReturnInfoSetupInput &input,
                                      const LowerInferenceReturnInfoSetupInput &returnInfoSetupInput,
                                      std::string &errorOut) {
  if (input.program == nullptr) {
    errorOut = "native backend missing inference get-return-info setup dependency: program";
    return false;
  }
  if (input.defMap == nullptr) {
    errorOut = "native backend missing inference get-return-info setup dependency: defMap";
    return false;
  }
  if (input.returnInfoCache == nullptr) {
    errorOut = "native backend missing inference get-return-info setup dependency: returnInfoCache";
    return false;
  }

  auto &returnInfoCache = *input.returnInfoCache;
  returnInfoCache.clear();

  const std::vector<semantics::ReturnDependencyComponent> returnDependencyOrder =
      semantics::buildReturnDependencyOrder(*input.program);
  for (const semantics::ReturnDependencyComponent &component : returnDependencyOrder) {
    std::vector<const Definition *> definitions =
        collectReturnInfoComponentDefinitions(component, *input.defMap);
    if (definitions.empty()) {
      continue;
    }

    for (const Definition *definition : definitions) {
      returnInfoCache.try_emplace(definition->fullPath, ReturnInfo{});
    }

    bool changed = false;
    do {
      changed = false;
      for (const Definition *definition : definitions) {
        ReturnInfo nextInfo;
        bool sawUnresolvedDependency = false;
        std::string localError;
        if (!runLowerInferenceReturnInfoSetupImpl(returnInfoSetupInput,
                                                  *definition,
                                                  nextInfo,
                                                  true,
                                                  &sawUnresolvedDependency,
                                                  localError)) {
          errorOut = std::move(localError);
          return false;
        }
        (void)sawUnresolvedDependency;

        auto cacheIt = returnInfoCache.find(definition->fullPath);
        if (cacheIt == returnInfoCache.end() || !returnInfoEquals(cacheIt->second, nextInfo)) {
          returnInfoCache[definition->fullPath] = nextInfo;
          changed = true;
        }
      }
    } while (changed);

    for (const Definition *definition : definitions) {
      ReturnInfo finalInfo;
      bool sawUnresolvedDependency = false;
      std::string localError;
      if (!runLowerInferenceReturnInfoSetupImpl(returnInfoSetupInput,
                                                *definition,
                                                finalInfo,
                                                true,
                                                &sawUnresolvedDependency,
                                                localError)) {
        errorOut = std::move(localError);
        return false;
      }
      if (sawUnresolvedDependency) {
        errorOut = component.hasCycle
                       ? "native backend return type inference requires explicit annotation on " + definition->fullPath
                       : "unable to infer return type on " + definition->fullPath;
        return false;
      }
      returnInfoCache[definition->fullPath] = finalInfo;
    }
  }

  return true;
}

} // namespace

bool runLowerInferenceReturnInfoSetup(const LowerInferenceReturnInfoSetupInput &input,
                                      const Definition &definition,
                                      ReturnInfo &infoInOut,
                                      std::string &errorOut) {
  return runLowerInferenceReturnInfoSetupImpl(input, definition, infoInOut, false, nullptr, errorOut);
}

bool runLowerInferenceGetReturnInfoStep(const LowerInferenceGetReturnInfoStepInput &input,
                                        const std::string &path,
                                        ReturnInfo &outInfo,
                                        std::string &errorOut) {
  if (input.defMap == nullptr) {
    errorOut = "native backend missing inference get-return-info step dependency: defMap";
    return false;
  }
  if (input.returnInfoCache == nullptr) {
    errorOut = "native backend missing inference get-return-info step dependency: returnInfoCache";
    return false;
  }
  if (input.returnInferenceStack == nullptr) {
    errorOut = "native backend missing inference get-return-info step dependency: returnInferenceStack";
    return false;
  }
  if (input.returnInfoSetupInput == nullptr) {
    errorOut = "native backend missing inference get-return-info step dependency: returnInfoSetupInput";
    return false;
  }

  auto &returnInfoCache = *input.returnInfoCache;
  auto &returnInferenceStack = *input.returnInferenceStack;

  auto cached = returnInfoCache.find(path);
  if (cached != returnInfoCache.end()) {
    outInfo = cached->second;
    return true;
  }

  auto defIt = input.defMap->find(path);
  if (defIt == input.defMap->end() || !defIt->second) {
    errorOut = "native backend cannot resolve definition: " + path;
    return false;
  }
  if (assignTypeDefinitionReturnInfo(*defIt->second, input.semanticProgram, outInfo)) {
    returnInfoCache.insert_or_assign(path, outInfo);
    return true;
  }
  if (input.semanticProgram != nullptr && input.semanticIndex != nullptr) {
    ReturnInfo info;
    if (!buildSemanticProductReturnInfo(
            *input.returnInfoSetupInput,
            *input.semanticProgram,
            *input.semanticIndex,
            *defIt->second,
            info,
            errorOut)) {
      return false;
    }
    returnInfoCache.insert_or_assign(path, info);
    outInfo = std::move(info);
    return true;
  }
  if (!returnInferenceStack.insert(path).second) {
    errorOut = "native backend return type inference requires explicit annotation on " + path;
    return false;
  }

  ReturnInfo info;
  if (!runLowerInferenceReturnInfoSetup(*input.returnInfoSetupInput, *defIt->second, info, errorOut)) {
    returnInferenceStack.erase(path);
    return false;
  }

  returnInferenceStack.erase(path);
  returnInfoCache.emplace(path, info);
  outInfo = info;
  return true;
}

bool runLowerInferenceGetReturnInfoCallbackSetup(const LowerInferenceGetReturnInfoCallbackSetupInput &input,
                                                 std::function<bool(const std::string &, ReturnInfo &)> &getReturnInfoOut,
                                                 std::string &errorOut) {
  getReturnInfoOut = {};
  if (input.defMap == nullptr) {
    errorOut = "native backend missing inference get-return-info callback setup dependency: defMap";
    return false;
  }
  if (input.returnInfoCache == nullptr) {
    errorOut = "native backend missing inference get-return-info callback setup dependency: returnInfoCache";
    return false;
  }
  if (input.returnInferenceStack == nullptr) {
    errorOut = "native backend missing inference get-return-info callback setup dependency: returnInferenceStack";
    return false;
  }
  if (input.returnInfoSetupInput == nullptr) {
    errorOut = "native backend missing inference get-return-info callback setup dependency: returnInfoSetupInput";
    return false;
  }
  if (input.error == nullptr) {
    errorOut = "native backend missing inference get-return-info callback setup dependency: error";
    return false;
  }

  const LowerInferenceGetReturnInfoStepInput stepInput = {
      .defMap = input.defMap,
      .returnInfoCache = input.returnInfoCache,
      .returnInferenceStack = input.returnInferenceStack,
      .returnInfoSetupInput = input.returnInfoSetupInput,
      .semanticProgram = input.semanticProgram,
      .semanticIndex = input.semanticIndex,
  };
  std::string *const inferenceError = input.error;
  getReturnInfoOut = [stepInput, inferenceError](const std::string &path, ReturnInfo &outInfo) -> bool {
    return runLowerInferenceGetReturnInfoStep(stepInput, path, outInfo, *inferenceError);
  };
  return true;
}

bool runLowerInferenceGetReturnInfoSetup(const LowerInferenceGetReturnInfoSetupInput &input,
                                         std::function<bool(const std::string &, ReturnInfo &)> &getReturnInfoOut,
                                         std::string &errorOut) {
  getReturnInfoOut = {};
  if (input.program == nullptr) {
    errorOut = "native backend missing inference get-return-info setup dependency: program";
    return false;
  }
  if (input.defMap == nullptr) {
    errorOut = "native backend missing inference get-return-info setup dependency: defMap";
    return false;
  }
  if (input.returnInfoCache == nullptr) {
    errorOut = "native backend missing inference get-return-info setup dependency: returnInfoCache";
    return false;
  }
  if (input.returnInferenceStack == nullptr) {
    errorOut = "native backend missing inference get-return-info setup dependency: returnInferenceStack";
    return false;
  }
  if (input.error == nullptr) {
    errorOut = "native backend missing inference get-return-info setup dependency: error";
    return false;
  }

  const Program *const program = input.program;
  const auto *defMap = input.defMap;
  auto *returnInfoCache = input.returnInfoCache;
  auto *returnInferenceStack = input.returnInferenceStack;
  const auto *semanticProgram = input.semanticProgram;
  const auto *semanticIndex = input.semanticIndex;
  std::string *const inferenceError = input.error;
  returnInferenceStack->clear();

  const LowerInferenceReturnInfoSetupInput returnInfoSetupInput = {
      .resolveStructTypeName = input.resolveStructTypeName,
      .resolveStructArrayInfoFromPath = input.resolveStructArrayInfoFromPath,
      .isBindingMutable = input.isBindingMutable,
      .bindingKind = input.bindingKind,
      .hasExplicitBindingTypeTransform = input.hasExplicitBindingTypeTransform,
      .bindingValueKind = input.bindingValueKind,
      .inferExprKind = input.inferExprKind,
      .isFileErrorBinding = input.isFileErrorBinding,
      .setReferenceArrayInfo = input.setReferenceArrayInfo,
      .applyStructArrayInfo = input.applyStructArrayInfo,
      .applyStructValueInfo = input.applyStructValueInfo,
      .inferStructExprPath = input.inferStructExprPath,
      .isStringBinding = input.isStringBinding,
      .inferArrayElementKind = input.inferArrayElementKind,
      .lowerMatchToIf = input.lowerMatchToIf,
  };
  getReturnInfoOut = [defMap, returnInfoCache, returnInferenceStack, semanticProgram, semanticIndex, returnInfoSetupInput, inferenceError](
                         const std::string &path, ReturnInfo &outInfo) -> bool {
    auto cached = returnInfoCache->find(path);
    if (cached != returnInfoCache->end()) {
      outInfo = cached->second;
      return true;
    }
    return runLowerInferenceGetReturnInfoStep(
        {
            .defMap = defMap,
            .returnInfoCache = returnInfoCache,
            .returnInferenceStack = returnInferenceStack,
            .returnInfoSetupInput = &returnInfoSetupInput,
            .semanticProgram = semanticProgram,
            .semanticIndex = semanticIndex,
        },
        path,
        outInfo,
        *inferenceError);
  };
  const LowerInferenceGetReturnInfoSetupInput precomputeInput = {
      .program = program,
      .defMap = defMap,
      .returnInfoCache = returnInfoCache,
      .returnInferenceStack = returnInferenceStack,
      .semanticProgram = semanticProgram,
      .semanticIndex = semanticIndex,
      .resolveStructTypeName = input.resolveStructTypeName,
      .resolveStructArrayInfoFromPath = input.resolveStructArrayInfoFromPath,
      .isBindingMutable = input.isBindingMutable,
      .bindingKind = input.bindingKind,
      .hasExplicitBindingTypeTransform = input.hasExplicitBindingTypeTransform,
      .bindingValueKind = input.bindingValueKind,
      .inferExprKind = input.inferExprKind,
      .isFileErrorBinding = input.isFileErrorBinding,
      .setReferenceArrayInfo = input.setReferenceArrayInfo,
      .applyStructArrayInfo = input.applyStructArrayInfo,
      .applyStructValueInfo = input.applyStructValueInfo,
      .inferStructExprPath = input.inferStructExprPath,
      .isStringBinding = input.isStringBinding,
      .inferArrayElementKind = input.inferArrayElementKind,
      .lowerMatchToIf = input.lowerMatchToIf,
      .error = inferenceError,
  };
  const bool useSemanticProductReturnInfo =
      semanticProgram != nullptr && semanticIndex != nullptr;
  if (!(useSemanticProductReturnInfo
            ? precomputeSemanticProductReturnInfoCache(precomputeInput, returnInfoSetupInput, errorOut)
            : precomputeOrderedReturnInfoCache(precomputeInput, returnInfoSetupInput, errorOut))) {
    return false;
  }
  errorOut.clear();
  return true;
}

} // namespace primec::ir_lowerer
