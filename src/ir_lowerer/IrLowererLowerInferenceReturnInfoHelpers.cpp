#include "IrLowererLowerInferenceSetup.h"

#include "../semantics/CondensationDag.h"
#include "../semantics/TypeResolutionGraph.h"

#include "IrLowererSetupTypeHelpers.h"

#include <algorithm>
#include <limits>
#include <vector>

namespace primec::ir_lowerer {

namespace {

bool returnInfoEquals(const ReturnInfo &left, const ReturnInfo &right) {
  return left.returnsVoid == right.returnsVoid && left.returnsArray == right.returnsArray &&
         left.kind == right.kind && left.isResult == right.isResult &&
         left.resultHasValue == right.resultHasValue &&
         left.resultValueKind == right.resultValueKind &&
         left.resultErrorType == right.resultErrorType;
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

bool componentHasReturnInferenceCycle(const semantics::CondensationDagNode &componentNode) {
  return componentNode.memberNodeIds.size() > 1;
}

std::vector<const Definition *> collectReturnInfoComponentDefinitions(
    const semantics::TypeResolutionGraph &graph,
    const semantics::CondensationDagNode &componentNode,
    const std::unordered_map<std::string, const Definition *> &defMap) {
  std::vector<const Definition *> definitions;
  definitions.reserve(componentNode.memberNodeIds.size());
  for (uint32_t nodeId : componentNode.memberNodeIds) {
    if (nodeId >= graph.nodes.size()) {
      continue;
    }
    const semantics::TypeResolutionGraphNode &node = graph.nodes[nodeId];
    if (node.kind != semantics::TypeResolutionNodeKind::DefinitionReturn) {
      continue;
    }
    const auto defIt = defMap.find(node.resolvedPath);
    if (defIt == defMap.end() || defIt->second == nullptr) {
      continue;
    }
    definitions.push_back(defIt->second);
  }
  sortDefinitionsForDeterministicReturnSolve(definitions);
  return definitions;
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

bool precomputeGraphReturnInfoCache(const LowerInferenceGetReturnInfoSetupInput &input,
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

  const semantics::TypeResolutionGraph graph = semantics::buildTypeResolutionGraph(*input.program);
  const semantics::CondensationDag dag = semantics::computeTypeResolutionDependencyDag(graph);

  for (auto componentIt = dag.topologicalComponentIds.rbegin(); componentIt != dag.topologicalComponentIds.rend();
       ++componentIt) {
    const semantics::CondensationDagNode &componentNode = dag.nodes[*componentIt];
    std::vector<const Definition *> definitions =
        collectReturnInfoComponentDefinitions(graph, componentNode, *input.defMap);
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

    const bool componentHasCycle = componentHasReturnInferenceCycle(componentNode);
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
        errorOut = componentHasCycle
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
  getReturnInfoOut = [defMap, returnInfoCache, returnInferenceStack, returnInfoSetupInput, inferenceError](
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
        },
        path,
        outInfo,
        *inferenceError);
  };
  if (!precomputeGraphReturnInfoCache(
          {
              .program = program,
              .defMap = defMap,
              .returnInfoCache = returnInfoCache,
              .returnInferenceStack = returnInferenceStack,
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
          },
          returnInfoSetupInput,
          errorOut)) {
    return false;
  }
  errorOut.clear();
  return true;
}

} // namespace primec::ir_lowerer
