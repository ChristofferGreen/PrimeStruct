#include "IrLowererLowerInferenceSetup.h"

#include "IrLowererHelpers.h"
#include "IrLowererSetupTypeHelpers.h"
#include "IrLowererUninitializedTypeHelpers.h"

namespace primec::ir_lowerer {

namespace {

bool inferLiteralOrNameExprKindImpl(const Expr &expr,
                                    const LocalMap &localsIn,
                                    const GetSetupMathConstantNameFn &getMathConstantName,
                                    LocalInfo::ValueKind &kindOut) {
  kindOut = LocalInfo::ValueKind::Unknown;
  switch (expr.kind) {
    case Expr::Kind::Literal:
      if (expr.isUnsigned) {
        kindOut = LocalInfo::ValueKind::UInt64;
      } else if (expr.intWidth == 64) {
        kindOut = LocalInfo::ValueKind::Int64;
      } else {
        kindOut = LocalInfo::ValueKind::Int32;
      }
      return true;
    case Expr::Kind::FloatLiteral:
      kindOut = (expr.floatWidth == 64) ? LocalInfo::ValueKind::Float64 : LocalInfo::ValueKind::Float32;
      return true;
    case Expr::Kind::BoolLiteral:
      kindOut = LocalInfo::ValueKind::Bool;
      return true;
    case Expr::Kind::StringLiteral:
      kindOut = LocalInfo::ValueKind::String;
      return true;
    case Expr::Kind::Name: {
      auto it = localsIn.find(expr.name);
      if (it == localsIn.end()) {
        std::string mathConstant;
        if (getMathConstantName(expr.name, mathConstant)) {
          kindOut = LocalInfo::ValueKind::Float64;
        }
        return true;
      }
      if (it->second.kind == LocalInfo::Kind::Value) {
        kindOut = it->second.valueKind;
        return true;
      }
      if (it->second.kind == LocalInfo::Kind::Reference) {
        kindOut = (it->second.referenceToArray || it->second.referenceToMap)
                      ? LocalInfo::ValueKind::Unknown
                      : it->second.valueKind;
        return true;
      }
      kindOut = LocalInfo::ValueKind::Unknown;
      return true;
    }
    default:
      return false;
  }
}

bool inferCallExprBaseKindImpl(const Expr &expr,
                               const LocalMap &localsIn,
                               const InferStructExprWithLocalsFn &inferStructExprPath,
                               const ResolveStructFieldSlotFn &resolveStructFieldSlot,
                               const std::function<bool(const Expr &,
                                                        const LocalMap &,
                                                        UninitializedStorageAccessInfo &,
                                                        bool &)> &resolveUninitializedStorage,
                               LocalInfo::ValueKind &kindOut) {
  kindOut = LocalInfo::ValueKind::Unknown;
  if (expr.kind != Expr::Kind::Call) {
    return false;
  }
  if (expr.isFieldAccess) {
    if (expr.args.size() != 1) {
      return true;
    }
    const Expr &receiver = expr.args.front();
    std::string structPath = inferStructExprPath(receiver, localsIn);
    if (structPath.empty()) {
      return true;
    }
    StructSlotFieldInfo fieldInfo;
    if (!resolveStructFieldSlot(structPath, expr.name, fieldInfo)) {
      return true;
    }
    kindOut = fieldInfo.structPath.empty() ? fieldInfo.valueKind : LocalInfo::ValueKind::Unknown;
    return true;
  }
  if (!expr.isMethodCall && (isSimpleCallName(expr, "take") || isSimpleCallName(expr, "borrow")) &&
      expr.args.size() == 1) {
    UninitializedStorageAccessInfo access;
    bool resolved = false;
    if (!resolveUninitializedStorage(expr.args.front(), localsIn, access, resolved)) {
      return true;
    }
    if (!resolved) {
      return false;
    }
    if (access.typeInfo.kind == LocalInfo::Kind::Value && access.typeInfo.structPath.empty() &&
        access.typeInfo.valueKind != LocalInfo::ValueKind::Unknown) {
      kindOut = access.typeInfo.valueKind;
    }
    return true;
  }
  if (expr.isMethodCall) {
    if (!expr.args.empty() && expr.args.front().kind == Expr::Kind::Name && expr.args.front().name == "Result") {
      if (expr.name == "ok") {
        kindOut = expr.args.size() > 1 ? LocalInfo::ValueKind::Int64 : LocalInfo::ValueKind::Int32;
        return true;
      }
      if (expr.name == "why") {
        kindOut = LocalInfo::ValueKind::String;
        return true;
      }
    }
    if (!expr.args.empty() && expr.name == "why") {
      const Expr &receiver = expr.args.front();
      if (receiver.kind == Expr::Kind::Name) {
        if (receiver.name == "FileError") {
          kindOut = LocalInfo::ValueKind::String;
          return true;
        }
        auto it = localsIn.find(receiver.name);
        if (it != localsIn.end() && it->second.isFileError) {
          kindOut = LocalInfo::ValueKind::String;
          return true;
        }
      }
    }
    if (!expr.args.empty() && expr.args.front().kind == Expr::Kind::Name) {
      auto it = localsIn.find(expr.args.front().name);
      if (it != localsIn.end() && it->second.isFileHandle) {
        if (expr.name == "write" || expr.name == "write_line" || expr.name == "write_byte" || expr.name == "read_byte" ||
            expr.name == "write_bytes" || expr.name == "flush" || expr.name == "close") {
          kindOut = LocalInfo::ValueKind::Int32;
          return true;
        }
      }
    }
    return false;
  }
  if (isSimpleCallName(expr, "File")) {
    kindOut = LocalInfo::ValueKind::Int64;
    return true;
  }
  if (isSimpleCallName(expr, "try") && expr.args.size() == 1) {
    const Expr &arg = expr.args.front();
    if (arg.kind == Expr::Kind::Name) {
      auto it = localsIn.find(arg.name);
      if (it != localsIn.end() && it->second.isResult) {
        kindOut = it->second.resultHasValue ? LocalInfo::ValueKind::Int64 : LocalInfo::ValueKind::Int32;
        return true;
      }
    }
    if (arg.kind == Expr::Kind::Call) {
      if (!arg.isMethodCall && isSimpleCallName(arg, "File")) {
        kindOut = LocalInfo::ValueKind::Int64;
        return true;
      }
      if (arg.isMethodCall && !arg.args.empty() && arg.args.front().kind == Expr::Kind::Name) {
        auto it = localsIn.find(arg.args.front().name);
        if (it != localsIn.end() && it->second.isFileHandle) {
          if (arg.name == "write" || arg.name == "write_line" || arg.name == "write_byte" || arg.name == "read_byte" ||
              arg.name == "write_bytes" || arg.name == "flush" || arg.name == "close") {
            kindOut = LocalInfo::ValueKind::Int32;
            return true;
          }
        }
        if (arg.args.front().name == "Result" && arg.name == "ok") {
          kindOut = arg.args.size() > 1 ? LocalInfo::ValueKind::Int64 : LocalInfo::ValueKind::Int32;
          return true;
        }
      }
    }
  }
  return false;
}

} // namespace

bool runLowerInferenceSetupBootstrap(const LowerInferenceSetupBootstrapInput &input,
                                     LowerInferenceSetupBootstrapState &stateOut,
                                     std::string &errorOut) {
  stateOut = LowerInferenceSetupBootstrapState{};

  if (input.defMap == nullptr) {
    errorOut = "native backend missing inference setup bootstrap dependency: defMap";
    return false;
  }
  if (input.importAliases == nullptr) {
    errorOut = "native backend missing inference setup bootstrap dependency: importAliases";
    return false;
  }
  if (input.structNames == nullptr) {
    errorOut = "native backend missing inference setup bootstrap dependency: structNames";
    return false;
  }
  if (!input.isArrayCountCall) {
    errorOut = "native backend missing inference setup bootstrap dependency: isArrayCountCall";
    return false;
  }
  if (!input.isVectorCapacityCall) {
    errorOut = "native backend missing inference setup bootstrap dependency: isVectorCapacityCall";
    return false;
  }
  if (!input.isEntryArgsName) {
    errorOut = "native backend missing inference setup bootstrap dependency: isEntryArgsName";
    return false;
  }
  if (!input.resolveExprPath) {
    errorOut = "native backend missing inference setup bootstrap dependency: resolveExprPath";
    return false;
  }
  if (!input.getBuiltinOperatorName) {
    errorOut = "native backend missing inference setup bootstrap dependency: getBuiltinOperatorName";
    return false;
  }

  const auto *defMap = input.defMap;
  const auto *importAliases = input.importAliases;
  const auto *structNames = input.structNames;
  const auto isArrayCountCall = input.isArrayCountCall;
  const auto isVectorCapacityCall = input.isVectorCapacityCall;
  const auto isEntryArgsName = input.isEntryArgsName;
  const auto resolveExprPath = input.resolveExprPath;
  const auto getBuiltinOperatorName = input.getBuiltinOperatorName;

  stateOut.resolveMethodCallDefinition = [defMap,
                                          importAliases,
                                          structNames,
                                          isArrayCountCall,
                                          isVectorCapacityCall,
                                          isEntryArgsName,
                                          resolveExprPath,
                                          &stateOut,
                                          &errorOut](const Expr &callExpr,
                                                     const LocalMap &localsIn) -> const Definition * {
    return resolveMethodCallDefinitionFromExpr(callExpr,
                                               localsIn,
                                               isArrayCountCall,
                                               isVectorCapacityCall,
                                               isEntryArgsName,
                                               *importAliases,
                                               *structNames,
                                               stateOut.inferExprKind,
                                               resolveExprPath,
                                               stateOut.getReturnInfo,
                                               *defMap,
                                               errorOut);
  };

  stateOut.inferPointerTargetKind = [getBuiltinOperatorName](const Expr &expr,
                                                              const LocalMap &localsIn) -> LocalInfo::ValueKind {
    return inferPointerTargetValueKind(
        expr,
        localsIn,
        [&](const Expr &candidate, std::string &builtinName) {
          return getBuiltinOperatorName(candidate, builtinName);
        });
  };

  return true;
}

bool runLowerInferenceSetup(const LowerInferenceSetupInput &input,
                            LowerInferenceSetupBootstrapState &stateOut,
                            std::string &errorOut) {
  if (!runLowerInferenceSetupBootstrap(
          {
              .defMap = input.defMap,
              .importAliases = input.importAliases,
              .structNames = input.structNames,
              .isArrayCountCall = input.isArrayCountCall,
              .isVectorCapacityCall = input.isVectorCapacityCall,
              .isEntryArgsName = input.isEntryArgsName,
              .resolveExprPath = input.resolveExprPath,
              .getBuiltinOperatorName = input.getBuiltinOperatorName,
          },
          stateOut,
          errorOut)) {
    return false;
  }

  if (!runLowerInferenceArrayKindSetup(
          {
              .defMap = input.defMap,
              .resolveExprPath = input.resolveExprPath,
              .resolveStructArrayInfoFromPath = input.resolveStructArrayInfoFromPath,
              .isArrayCountCall = input.isArrayCountCall,
              .isStringCountCall = input.isStringCountCall,
          },
          stateOut,
          errorOut)) {
    return false;
  }
  if (!runLowerInferenceExprKindCallBaseSetup(
          {
              .inferStructExprPath = input.inferStructExprPath,
              .resolveStructFieldSlot = input.resolveStructFieldSlot,
              .resolveUninitializedStorage = input.resolveUninitializedStorage,
          },
          stateOut,
          errorOut)) {
    return false;
  }
  if (!runLowerInferenceExprKindCallReturnSetup(
          {
              .defMap = input.defMap,
              .resolveExprPath = input.resolveExprPath,
              .isArrayCountCall = input.isArrayCountCall,
              .isStringCountCall = input.isStringCountCall,
          },
          stateOut,
          errorOut)) {
    return false;
  }
  if (!runLowerInferenceExprKindCallFallbackSetup(
          {
              .defMap = input.defMap,
              .resolveExprPath = input.resolveExprPath,
              .isArrayCountCall = input.isArrayCountCall,
              .isStringCountCall = input.isStringCountCall,
              .isVectorCapacityCall = input.isVectorCapacityCall,
              .isEntryArgsName = input.isEntryArgsName,
          },
          stateOut,
          errorOut)) {
    return false;
  }
  if (!runLowerInferenceExprKindCallOperatorFallbackSetup(
          {
              .hasMathImport = input.hasMathImport,
              .combineNumericKinds = input.combineNumericKinds,
          },
          stateOut,
          errorOut)) {
    return false;
  }
  if (!runLowerInferenceExprKindCallControlFlowFallbackSetup(
          {
              .defMap = input.defMap,
              .resolveExprPath = input.resolveExprPath,
              .lowerMatchToIf = input.lowerMatchToIf,
              .combineNumericKinds = input.combineNumericKinds,
              .isBindingMutable = input.isBindingMutable,
              .bindingKind = input.bindingKind,
              .hasExplicitBindingTypeTransform = input.hasExplicitBindingTypeTransform,
              .bindingValueKind = input.bindingValueKind,
              .applyStructArrayInfo = input.applyStructArrayInfo,
              .applyStructValueInfo = input.applyStructValueInfo,
              .inferStructExprPath = input.inferStructExprPath,
          },
          stateOut,
          errorOut)) {
    return false;
  }
  if (!runLowerInferenceExprKindCallPointerFallbackSetup(
          {},
          stateOut,
          errorOut)) {
    return false;
  }
  if (!runLowerInferenceExprKindBaseSetup(
          {
              .getMathConstantName = input.getMathConstantName,
          },
          stateOut,
          errorOut)) {
    return false;
  }
  if (!runLowerInferenceExprKindDispatchSetup(
          {
              .error = &errorOut,
          },
          stateOut,
          errorOut)) {
    return false;
  }

  auto &returnInfoCache = stateOut.returnInfoCache;
  auto &returnInferenceStack = stateOut.returnInferenceStack;
  auto &inferExprKind = stateOut.inferExprKind;
  auto &inferArrayElementKind = stateOut.inferArrayElementKind;
  return runLowerInferenceGetReturnInfoSetup(
      {
          .defMap = input.defMap,
          .returnInfoCache = &returnInfoCache,
          .returnInferenceStack = &returnInferenceStack,
          .resolveStructTypeName = input.resolveStructTypeName,
          .resolveStructArrayInfoFromPath = input.resolveStructArrayInfoFromPath,
          .isBindingMutable = input.isBindingMutable,
          .bindingKind = input.bindingKind,
          .hasExplicitBindingTypeTransform = input.hasExplicitBindingTypeTransform,
          .bindingValueKind = input.bindingValueKind,
          .inferExprKind = inferExprKind,
          .isFileErrorBinding = input.isFileErrorBinding,
          .setReferenceArrayInfo = input.setReferenceArrayInfo,
          .applyStructArrayInfo = input.applyStructArrayInfo,
          .applyStructValueInfo = input.applyStructValueInfo,
          .inferStructExprPath = input.inferStructExprPath,
          .isStringBinding = input.isStringBinding,
          .inferArrayElementKind = inferArrayElementKind,
          .lowerMatchToIf = input.lowerMatchToIf,
          .error = &errorOut,
      },
      stateOut.getReturnInfo,
      errorOut);
}

bool runLowerInferenceArrayKindSetup(const LowerInferenceArrayKindSetupInput &input,
                                     LowerInferenceSetupBootstrapState &stateInOut,
                                     std::string &errorOut) {
  if (input.defMap == nullptr) {
    errorOut = "native backend missing inference array-kind setup dependency: defMap";
    return false;
  }
  if (!input.resolveExprPath) {
    errorOut = "native backend missing inference array-kind setup dependency: resolveExprPath";
    return false;
  }
  if (!input.resolveStructArrayInfoFromPath) {
    errorOut = "native backend missing inference array-kind setup dependency: resolveStructArrayInfoFromPath";
    return false;
  }
  if (!input.isArrayCountCall) {
    errorOut = "native backend missing inference array-kind setup dependency: isArrayCountCall";
    return false;
  }
  if (!input.isStringCountCall) {
    errorOut = "native backend missing inference array-kind setup dependency: isStringCountCall";
    return false;
  }
  if (!stateInOut.resolveMethodCallDefinition) {
    errorOut = "native backend missing inference array-kind setup state: resolveMethodCallDefinition";
    return false;
  }

  const auto *defMap = input.defMap;
  const auto resolveExprPath = input.resolveExprPath;
  const auto resolveStructArrayInfoFromPath = input.resolveStructArrayInfoFromPath;
  const auto isArrayCountCall = input.isArrayCountCall;
  const auto isStringCountCall = input.isStringCountCall;
  auto isZeroFieldStructDef = [](const Definition &def) -> bool {
    if (!isStructDefinition(def)) {
      return false;
    }
    for (const auto &stmt : def.statements) {
      if (!stmt.isBinding) {
        return false;
      }
      bool isStaticField = false;
      for (const auto &transform : stmt.transforms) {
        if (transform.name == "static") {
          isStaticField = true;
          break;
        }
      }
      if (!isStaticField) {
        return false;
      }
    }
    return true;
  };

  stateInOut.inferBufferElementKind = [&stateInOut](const Expr &expr,
                                                     const LocalMap &localsIn) -> LocalInfo::ValueKind {
    return inferBufferElementValueKind(
        expr,
        localsIn,
        [&](const Expr &candidate, const LocalMap &candidateLocals) {
          return stateInOut.inferArrayElementKind(candidate, candidateLocals);
        });
  };

  stateInOut.inferArrayElementKind = [defMap,
                                      resolveExprPath,
                                      resolveStructArrayInfoFromPath,
                                      isArrayCountCall,
                                      isStringCountCall,
                                      isZeroFieldStructDef,
                                      &stateInOut](const Expr &expr,
                                                   const LocalMap &localsIn) -> LocalInfo::ValueKind {
    return inferArrayElementValueKind(
        expr,
        localsIn,
        [&](const Expr &candidate, const LocalMap &candidateLocals) {
          return stateInOut.inferBufferElementKind(candidate, candidateLocals);
        },
        [&](const Expr &candidate) { return resolveExprPath(candidate); },
        [&](const std::string &structPath, LocalInfo::ValueKind &kindOut) {
          StructArrayTypeInfo structInfo;
          if (resolveStructArrayInfoFromPath(structPath, structInfo)) {
            kindOut = structInfo.elementKind;
            return true;
          }
          auto defIt = defMap->find(structPath);
          if (defIt != defMap->end() && defIt->second != nullptr && isZeroFieldStructDef(*defIt->second)) {
            kindOut = LocalInfo::ValueKind::Int32;
            return true;
          }
          return false;
        },
        [&](const Expr &candidate, const LocalMap &, LocalInfo::ValueKind &kindOut) {
          return resolveDefinitionCallReturnKind(
              candidate, *defMap, resolveExprPath, stateInOut.getReturnInfo, true, kindOut);
        },
        [&](const Expr &candidate, const LocalMap &candidateLocals, LocalInfo::ValueKind &kindOut) {
          return resolveCountMethodCallReturnKind(candidate,
                                                  candidateLocals,
                                                  isArrayCountCall,
                                                  isStringCountCall,
                                                  stateInOut.resolveMethodCallDefinition,
                                                  stateInOut.getReturnInfo,
                                                  true,
                                                  kindOut,
                                                  nullptr,
                                                  stateInOut.inferExprKind);
        },
        [&](const Expr &candidate, const LocalMap &candidateLocals, LocalInfo::ValueKind &kindOut) {
          return resolveMethodCallReturnKind(
              candidate, candidateLocals, stateInOut.resolveMethodCallDefinition, stateInOut.getReturnInfo, true, kindOut);
        });
  };
  return true;
}

bool runLowerInferenceExprKindBaseSetup(const LowerInferenceExprKindBaseSetupInput &input,
                                        LowerInferenceSetupBootstrapState &stateInOut,
                                        std::string &errorOut) {
  if (!input.getMathConstantName) {
    errorOut = "native backend missing inference expr-kind base setup dependency: getMathConstantName";
    return false;
  }

  const auto getMathConstantName = input.getMathConstantName;
  stateInOut.inferLiteralOrNameExprKind =
      [getMathConstantName](const Expr &expr, const LocalMap &localsIn, LocalInfo::ValueKind &kindOut) {
        return inferLiteralOrNameExprKindImpl(expr, localsIn, getMathConstantName, kindOut);
      };
  return true;
}

bool runLowerInferenceExprKindCallBaseSetup(const LowerInferenceExprKindCallBaseSetupInput &input,
                                            LowerInferenceSetupBootstrapState &stateInOut,
                                            std::string &errorOut) {
  if (!input.inferStructExprPath) {
    errorOut = "native backend missing inference expr-kind call-base setup dependency: inferStructExprPath";
    return false;
  }
  if (!input.resolveStructFieldSlot) {
    errorOut = "native backend missing inference expr-kind call-base setup dependency: resolveStructFieldSlot";
    return false;
  }
  if (!input.resolveUninitializedStorage) {
    errorOut = "native backend missing inference expr-kind call-base setup dependency: resolveUninitializedStorage";
    return false;
  }

  const auto inferStructExprPath = input.inferStructExprPath;
  const auto resolveStructFieldSlot = input.resolveStructFieldSlot;
  const auto resolveUninitializedStorage = input.resolveUninitializedStorage;
  stateInOut.inferCallExprBaseKind =
      [inferStructExprPath, resolveStructFieldSlot, resolveUninitializedStorage](
          const Expr &expr, const LocalMap &localsIn, LocalInfo::ValueKind &kindOut) {
        return inferCallExprBaseKindImpl(
            expr, localsIn, inferStructExprPath, resolveStructFieldSlot, resolveUninitializedStorage, kindOut);
      };
  return true;
}

bool runLowerInferenceExprKindCallReturnSetup(const LowerInferenceExprKindCallReturnSetupInput &input,
                                              LowerInferenceSetupBootstrapState &stateInOut,
                                              std::string &errorOut) {
  if (input.defMap == nullptr) {
    errorOut = "native backend missing inference expr-kind call-return setup dependency: defMap";
    return false;
  }
  if (!input.resolveExprPath) {
    errorOut = "native backend missing inference expr-kind call-return setup dependency: resolveExprPath";
    return false;
  }
  if (!input.isArrayCountCall) {
    errorOut = "native backend missing inference expr-kind call-return setup dependency: isArrayCountCall";
    return false;
  }
  if (!input.isStringCountCall) {
    errorOut = "native backend missing inference expr-kind call-return setup dependency: isStringCountCall";
    return false;
  }
  if (!stateInOut.resolveMethodCallDefinition) {
    errorOut = "native backend missing inference expr-kind call-return setup state: resolveMethodCallDefinition";
    return false;
  }

  const auto *defMap = input.defMap;
  const auto resolveExprPath = input.resolveExprPath;
  const auto isArrayCountCall = input.isArrayCountCall;
  const auto isStringCountCall = input.isStringCountCall;
  auto isNamespacedCollectionReceiverProbeCall = [](const Expr &candidate) {
    if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall || candidate.name.empty()) {
      return false;
    }
    std::string collectionName;
    std::string helperName;
    if (!getNamespacedCollectionHelperName(candidate, collectionName, helperName)) {
      return false;
    }
    if (collectionName == "vector") {
      if (helperName == "at" || helperName == "at_unsafe" || helperName == "push" || helperName == "reserve" ||
          helperName == "remove_at" || helperName == "remove_swap") {
        return candidate.args.size() == 2;
      }
      if (helperName == "count" || helperName == "capacity" || helperName == "pop" || helperName == "clear") {
        return candidate.args.size() == 1;
      }
      return false;
    }
    if (collectionName != "map") {
      return false;
    }
    if (helperName == "at" || helperName == "at_unsafe") {
      return candidate.args.size() == 2;
    }
    if (helperName == "count") {
      return candidate.args.size() == 1;
    }
    return false;
  };
  auto resolveDefinitionCallReturnKindForCandidate =
      [defMap, resolveExprPath, &stateInOut](const Expr &candidate,
                                             LocalInfo::ValueKind &candidateKindOut,
                                             bool &matchedOut) {
        bool definitionMatched = false;
        const bool resolved = resolveDefinitionCallReturnKind(
            candidate, *defMap, resolveExprPath, stateInOut.getReturnInfo, false, candidateKindOut, &definitionMatched);
        matchedOut = definitionMatched;
        return resolved;
      };

  stateInOut.inferCallExprDirectReturnKind =
      [isArrayCountCall,
       isStringCountCall,
       &stateInOut,
       isNamespacedCollectionReceiverProbeCall,
       resolveDefinitionCallReturnKindForCandidate](
          const Expr &expr, const LocalMap &localsIn, LocalInfo::ValueKind &kindOut) {
        return resolveCallExpressionReturnKind(
            expr,
            localsIn,
            [&](const Expr &candidate, const LocalMap &, LocalInfo::ValueKind &candidateKindOut, bool &matchedOut) {
              if (isNamespacedCollectionReceiverProbeCall(candidate)) {
                matchedOut = false;
                return false;
              }
              return resolveDefinitionCallReturnKindForCandidate(candidate, candidateKindOut, matchedOut);
            },
            [&](const Expr &candidate,
                const LocalMap &candidateLocals,
                LocalInfo::ValueKind &candidateKindOut,
                bool &matchedOut) {
              bool countMethodResolved = false;
              if (resolveCountMethodCallReturnKind(candidate,
                                                   candidateLocals,
                                                   isArrayCountCall,
                                                   isStringCountCall,
                                                   stateInOut.resolveMethodCallDefinition,
                                                   stateInOut.getReturnInfo,
                                                   false,
                                                   candidateKindOut,
                                                   &countMethodResolved,
                                                   stateInOut.inferExprKind)) {
                matchedOut = countMethodResolved;
                return true;
              }
              if (countMethodResolved) {
                matchedOut = true;
                return false;
              }
              bool capacityMethodResolved = false;
              const bool capacityResolved = resolveCapacityMethodCallReturnKind(candidate,
                                                                                candidateLocals,
                                                                                stateInOut.resolveMethodCallDefinition,
                                                                                stateInOut.getReturnInfo,
                                                                                false,
                                                                                candidateKindOut,
                                                                                &capacityMethodResolved);
              if (capacityResolved) {
                matchedOut = true;
                return true;
              }
              if (capacityMethodResolved) {
                matchedOut = true;
                return false;
              }
              if (!isNamespacedCollectionReceiverProbeCall(candidate)) {
                matchedOut = false;
                return false;
              }
              return resolveDefinitionCallReturnKindForCandidate(candidate, candidateKindOut, matchedOut);
            },
            [&](const Expr &candidate,
                const LocalMap &candidateLocals,
                LocalInfo::ValueKind &candidateKindOut,
                bool &matchedOut) {
              bool methodResolved = false;
              const bool resolved = resolveMethodCallReturnKind(candidate,
                                                                candidateLocals,
                                                                stateInOut.resolveMethodCallDefinition,
                                                                stateInOut.getReturnInfo,
                                                                false,
                                                                candidateKindOut,
                                                                &methodResolved);
              matchedOut = methodResolved;
              return resolved;
            },
            kindOut);
      };
  return true;
}

bool runLowerInferenceReturnInfoSetup(const LowerInferenceReturnInfoSetupInput &input,
                                      const Definition &definition,
                                      ReturnInfo &infoInOut,
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
      ReturnInferenceOptions options;
      options.missingReturnBehavior = MissingReturnBehavior::Error;
      options.includeDefinitionReturnExpr = true;
      if (!ir_lowerer::inferDefinitionReturnType(
              definition,
              LocalMap{},
              inferBindingIntoLocals,
              [&](const Expr &expr, const LocalMap &localsIn) { return inferExprKind(expr, localsIn); },
              [&](const Expr &expr, const LocalMap &localsIn) { return inferArrayElementKind(expr, localsIn); },
              [&](const Expr &expr, Expr &expanded, std::string &inferError) {
                return lowerMatchToIf(expr, expanded, inferError);
              },
              options,
              infoInOut,
              errorOut)) {
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
    if (!ir_lowerer::inferDefinitionReturnType(
            definition,
            LocalMap{},
            inferBindingIntoLocals,
            [&](const Expr &expr, const LocalMap &localsIn) { return inferExprKind(expr, localsIn); },
            [&](const Expr &expr, const LocalMap &localsIn) { return inferArrayElementKind(expr, localsIn); },
            [&](const Expr &expr, Expr &expanded, std::string &inferError) {
              return lowerMatchToIf(expr, expanded, inferError);
            },
            options,
            infoInOut,
            errorOut)) {
      return false;
    }
  }

  return true;
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

  const auto *defMap = input.defMap;
  auto *returnInfoCache = input.returnInfoCache;
  auto *returnInferenceStack = input.returnInferenceStack;
  std::string *const inferenceError = input.error;

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
  errorOut.clear();
  return true;
}

bool runLowerInferenceExprKindCallFallbackSetup(const LowerInferenceExprKindCallFallbackSetupInput &input,
                                                LowerInferenceSetupBootstrapState &stateInOut,
                                                std::string &errorOut) {
  if (!input.isArrayCountCall) {
    errorOut = "native backend missing inference expr-kind call-fallback setup dependency: isArrayCountCall";
    return false;
  }
  if (!input.isStringCountCall) {
    errorOut = "native backend missing inference expr-kind call-fallback setup dependency: isStringCountCall";
    return false;
  }
  if (!input.isVectorCapacityCall) {
    errorOut = "native backend missing inference expr-kind call-fallback setup dependency: isVectorCapacityCall";
    return false;
  }
  if (!input.isEntryArgsName) {
    errorOut = "native backend missing inference expr-kind call-fallback setup dependency: isEntryArgsName";
    return false;
  }
  if (!stateInOut.inferBufferElementKind) {
    errorOut = "native backend missing inference expr-kind call-fallback setup state: inferBufferElementKind";
    return false;
  }

  const auto isArrayCountCall = input.isArrayCountCall;
  const auto isStringCountCall = input.isStringCountCall;
  const auto isVectorCapacityCall = input.isVectorCapacityCall;
  const auto isEntryArgsName = input.isEntryArgsName;
  const auto *defMap = input.defMap;
  const auto resolveExprPath = input.resolveExprPath;

  const auto resolveCallMapAccessValueKind =
      [defMap, resolveExprPath, &stateInOut](const Expr &candidate,
                                             const LocalMap &candidateLocals,
                                             LocalInfo::ValueKind &kindOut) {
        kindOut = LocalInfo::ValueKind::Unknown;
        if (candidate.kind != Expr::Kind::Call) {
          return false;
        }

        const Definition *callee = nullptr;
        if (candidate.isMethodCall) {
          if (!stateInOut.resolveMethodCallDefinition) {
            return false;
          }
          callee = stateInOut.resolveMethodCallDefinition(candidate, candidateLocals);
        } else if (defMap != nullptr && resolveExprPath) {
          const std::string path = resolveExprPath(candidate);
          auto it = defMap->find(path);
          if (it != defMap->end()) {
            callee = it->second;
          }
        }
        if (callee == nullptr) {
          return false;
        }

        std::string collectionName;
        std::vector<std::string> collectionArgs;
        if (!inferDeclaredReturnCollection(*callee, collectionName, collectionArgs) || collectionName != "map" ||
            collectionArgs.size() != 2) {
          return false;
        }

        kindOut = valueKindFromTypeName(collectionArgs.back());
        return true;
      };

  stateInOut.inferCallExprCountAccessGpuFallbackKind =
      [isArrayCountCall,
       isStringCountCall,
       isVectorCapacityCall,
       isEntryArgsName,
       resolveCallMapAccessValueKind,
       &stateInOut](
          const Expr &expr, const LocalMap &localsIn, LocalInfo::ValueKind &kindOut) {
        kindOut = LocalInfo::ValueKind::Unknown;

        auto isBuiltinCountLikeCall = [](const Expr &candidate) {
          if (candidate.kind != Expr::Kind::Call) {
            return false;
          }
          if (isSimpleCallName(candidate, "count")) {
            return true;
          }
          std::string collectionName;
          std::string helperName;
          return getNamespacedCollectionHelperName(candidate, collectionName, helperName) && helperName == "count";
        };

        if (isBuiltinCountLikeCall(expr) && expr.args.size() == 1) {
          LocalInfo::ValueKind accessElementKind = LocalInfo::ValueKind::Unknown;
          if (resolveArrayMapAccessElementKind(
                  expr.args.front(),
                  localsIn,
                  [&](const Expr &candidate, const LocalMap &candidateLocals) {
                    return isEntryArgsName(candidate, candidateLocals);
                  },
                  accessElementKind,
                  [&](const Expr &candidate, const LocalMap &candidateLocals, LocalInfo::ValueKind &candidateKindOut) {
                    return resolveCallMapAccessValueKind(candidate, candidateLocals, candidateKindOut);
                  }) == ArrayMapAccessElementKindResolution::Resolved &&
              accessElementKind == LocalInfo::ValueKind::String) {
            kindOut = LocalInfo::ValueKind::Int32;
            return true;
          }
          if (stateInOut.inferExprKind(expr.args.front(), localsIn) == LocalInfo::ValueKind::String) {
            kindOut = LocalInfo::ValueKind::Int32;
            return true;
          }
        }

        LocalInfo::ValueKind countCapacityKind = LocalInfo::ValueKind::Unknown;
        if (inferCountCapacityCallReturnKind(
                expr,
                localsIn,
                [&](const Expr &candidateExpr, const LocalMap &candidateLocals) {
                  return isArrayCountCall(candidateExpr, candidateLocals);
                },
                [&](const Expr &candidateExpr, const LocalMap &candidateLocals) {
                  return isStringCountCall(candidateExpr, candidateLocals);
                },
                [&](const Expr &candidateExpr, const LocalMap &candidateLocals) {
                  return isVectorCapacityCall(candidateExpr, candidateLocals);
                },
                countCapacityKind) == CountCapacityCallReturnKindResolution::Resolved) {
          kindOut = countCapacityKind;
          return true;
        }

        LocalInfo::ValueKind accessElementKind = LocalInfo::ValueKind::Unknown;
        if (resolveArrayMapAccessElementKind(
                expr,
                localsIn,
                [&](const Expr &candidate, const LocalMap &candidateLocals) {
                  return isEntryArgsName(candidate, candidateLocals);
                },
                accessElementKind,
                [&](const Expr &candidate, const LocalMap &candidateLocals, LocalInfo::ValueKind &candidateKindOut) {
                  return resolveCallMapAccessValueKind(candidate, candidateLocals, candidateKindOut);
                }) == ArrayMapAccessElementKindResolution::Resolved) {
          kindOut = accessElementKind;
          return true;
        }

        LocalInfo::ValueKind gpuBufferKind = LocalInfo::ValueKind::Unknown;
        if (inferGpuBufferCallReturnKind(
                expr,
                localsIn,
                [&](const Expr &candidateExpr, const LocalMap &candidateLocals) {
                  return stateInOut.inferBufferElementKind(candidateExpr, candidateLocals);
                },
                gpuBufferKind) == GpuBufferCallReturnKindResolution::Resolved) {
          kindOut = gpuBufferKind;
          return true;
        }
        return false;
      };
  return true;
}

bool runLowerInferenceExprKindCallOperatorFallbackSetup(
    const LowerInferenceExprKindCallOperatorFallbackSetupInput &input,
    LowerInferenceSetupBootstrapState &stateInOut,
    std::string &errorOut) {
  if (!input.combineNumericKinds) {
    errorOut = "native backend missing inference expr-kind call-operator fallback setup dependency: combineNumericKinds";
    return false;
  }
  if (!stateInOut.inferPointerTargetKind) {
    errorOut = "native backend missing inference expr-kind call-operator fallback setup state: inferPointerTargetKind";
    return false;
  }

  const bool hasMathImport = input.hasMathImport;
  const auto combineNumericKinds = input.combineNumericKinds;

  stateInOut.inferCallExprOperatorFallbackKind = [hasMathImport, combineNumericKinds, &stateInOut](
                                                     const Expr &expr, const LocalMap &localsIn, LocalInfo::ValueKind &kindOut) {
    kindOut = LocalInfo::ValueKind::Unknown;
    const auto inferExprKindOrUnknown = [&](const Expr &candidateExpr, const LocalMap &candidateLocals) {
      if (!stateInOut.inferExprKind) {
        return LocalInfo::ValueKind::Unknown;
      }
      return stateInOut.inferExprKind(candidateExpr, candidateLocals);
    };

    LocalInfo::ValueKind comparisonOperatorKind = LocalInfo::ValueKind::Unknown;
    if (inferComparisonOperatorCallReturnKind(
            expr,
            localsIn,
            [&](const Expr &candidateExpr, const LocalMap &candidateLocals) {
              return inferExprKindOrUnknown(candidateExpr, candidateLocals);
            },
            [&](LocalInfo::ValueKind left, LocalInfo::ValueKind right) {
              return combineNumericKinds(left, right);
            },
            comparisonOperatorKind) == ComparisonOperatorCallReturnKindResolution::Resolved) {
      kindOut = comparisonOperatorKind;
      return true;
    }

    LocalInfo::ValueKind mathBuiltinKind = LocalInfo::ValueKind::Unknown;
    if (inferMathBuiltinReturnKind(
            expr,
            localsIn,
            hasMathImport,
            [&](const Expr &candidateExpr, const LocalMap &candidateLocals) {
              return inferExprKindOrUnknown(candidateExpr, candidateLocals);
            },
            [&](LocalInfo::ValueKind left, LocalInfo::ValueKind right) {
              return combineNumericKinds(left, right);
            },
            mathBuiltinKind) == MathBuiltinReturnKindResolution::Resolved) {
      kindOut = mathBuiltinKind;
      return true;
    }

    LocalInfo::ValueKind nonMathScalarKind = LocalInfo::ValueKind::Unknown;
    if (inferNonMathScalarCallReturnKind(
            expr,
            localsIn,
            [&](const Expr &candidateExpr, const LocalMap &candidateLocals) {
              return inferExprKindOrUnknown(candidateExpr, candidateLocals);
            },
            [&](const Expr &candidateExpr, const LocalMap &candidateLocals) {
              return stateInOut.inferPointerTargetKind(candidateExpr, candidateLocals);
            },
            nonMathScalarKind) == NonMathScalarCallReturnKindResolution::Resolved) {
      kindOut = nonMathScalarKind;
      return true;
    }

    return false;
  };
  return true;
}

bool runLowerInferenceExprKindCallControlFlowFallbackSetup(
    const LowerInferenceExprKindCallControlFlowFallbackSetupInput &input,
    LowerInferenceSetupBootstrapState &stateInOut,
    std::string &errorOut) {
  if (input.defMap == nullptr) {
    errorOut = "native backend missing inference expr-kind call-control-flow fallback setup dependency: defMap";
    return false;
  }
  if (!input.resolveExprPath) {
    errorOut = "native backend missing inference expr-kind call-control-flow fallback setup dependency: resolveExprPath";
    return false;
  }
  if (!input.lowerMatchToIf) {
    errorOut = "native backend missing inference expr-kind call-control-flow fallback setup dependency: lowerMatchToIf";
    return false;
  }
  if (!input.combineNumericKinds) {
    errorOut = "native backend missing inference expr-kind call-control-flow fallback setup dependency: combineNumericKinds";
    return false;
  }
  if (!input.isBindingMutable) {
    errorOut = "native backend missing inference expr-kind call-control-flow fallback setup dependency: isBindingMutable";
    return false;
  }
  if (!input.bindingKind) {
    errorOut = "native backend missing inference expr-kind call-control-flow fallback setup dependency: bindingKind";
    return false;
  }
  if (!input.hasExplicitBindingTypeTransform) {
    errorOut =
        "native backend missing inference expr-kind call-control-flow fallback setup dependency: hasExplicitBindingTypeTransform";
    return false;
  }
  if (!input.bindingValueKind) {
    errorOut = "native backend missing inference expr-kind call-control-flow fallback setup dependency: bindingValueKind";
    return false;
  }
  if (!input.applyStructArrayInfo) {
    errorOut = "native backend missing inference expr-kind call-control-flow fallback setup dependency: applyStructArrayInfo";
    return false;
  }
  if (!input.applyStructValueInfo) {
    errorOut = "native backend missing inference expr-kind call-control-flow fallback setup dependency: applyStructValueInfo";
    return false;
  }
  if (!input.inferStructExprPath) {
    errorOut = "native backend missing inference expr-kind call-control-flow fallback setup dependency: inferStructExprPath";
    return false;
  }

  const auto *defMap = input.defMap;
  const auto resolveExprPath = input.resolveExprPath;
  const auto lowerMatchToIf = input.lowerMatchToIf;
  const auto combineNumericKinds = input.combineNumericKinds;
  const auto isBindingMutable = input.isBindingMutable;
  const auto bindingKind = input.bindingKind;
  const auto hasExplicitBindingTypeTransform = input.hasExplicitBindingTypeTransform;
  const auto bindingValueKind = input.bindingValueKind;
  const auto applyStructArrayInfo = input.applyStructArrayInfo;
  const auto applyStructValueInfo = input.applyStructValueInfo;
  const auto inferStructExprPath = input.inferStructExprPath;

  stateInOut.inferCallExprControlFlowFallbackKind = [defMap,
                                                     resolveExprPath,
                                                     lowerMatchToIf,
                                                     combineNumericKinds,
                                                     isBindingMutable,
                                                     bindingKind,
                                                     hasExplicitBindingTypeTransform,
                                                     bindingValueKind,
                                                     applyStructArrayInfo,
                                                     applyStructValueInfo,
                                                     inferStructExprPath,
                                                     &stateInOut](const Expr &expr,
                                                                  const LocalMap &localsIn,
                                                                  std::string &errorInOut,
                                                                  LocalInfo::ValueKind &kindOut) {
    kindOut = LocalInfo::ValueKind::Unknown;
    const auto inferExprKindOrUnknown = [&](const Expr &candidateExpr, const LocalMap &candidateLocals) {
      if (!stateInOut.inferExprKind) {
        return LocalInfo::ValueKind::Unknown;
      }
      return stateInOut.inferExprKind(candidateExpr, candidateLocals);
    };

    const auto resolution = inferControlFlowCallReturnKind(
        expr,
        localsIn,
        [&](const Expr &candidateExpr) { return resolveExprPath(candidateExpr); },
        [&](const Expr &candidateExpr, Expr &expandedExpr, std::string &candidateErrorOut) {
          return lowerMatchToIf(candidateExpr, expandedExpr, candidateErrorOut);
        },
        [&](const Expr &candidateExpr, const LocalMap &candidateLocals) {
          return inferExprKindOrUnknown(candidateExpr, candidateLocals);
        },
        [&](LocalInfo::ValueKind left, LocalInfo::ValueKind right) {
          return combineNumericKinds(left, right);
        },
        [&](const std::vector<Expr> &bodyExpressions, const LocalMap &localsBase) {
          return inferBodyValueKindWithLocalsScaffolding(
              bodyExpressions,
              localsBase,
              [&](const Expr &candidateExpr, const LocalMap &candidateLocals) {
                return inferExprKindOrUnknown(candidateExpr, candidateLocals);
              },
              [&](const Expr &candidateExpr) { return isBindingMutable(candidateExpr); },
              [&](const Expr &candidateExpr) { return bindingKind(candidateExpr); },
              [&](const Expr &candidateExpr) { return hasExplicitBindingTypeTransform(candidateExpr); },
              [&](const Expr &candidateExpr, LocalInfo::Kind kind) {
                return bindingValueKind(candidateExpr, kind);
              },
              [&](const Expr &candidateExpr, LocalInfo &info) { applyStructArrayInfo(candidateExpr, info); },
              [&](const Expr &candidateExpr, LocalInfo &info) { applyStructValueInfo(candidateExpr, info); },
              [&](const Expr &candidateExpr, const LocalMap &candidateLocals) {
                return inferStructExprPath(candidateExpr, candidateLocals);
              });
        },
        [&](const std::string &path) { return defMap->find(path) != defMap->end(); },
        errorInOut,
        kindOut);
    return resolution == ControlFlowCallReturnKindResolution::Resolved;
  };

  return true;
}

bool runLowerInferenceExprKindCallPointerFallbackSetup(
    const LowerInferenceExprKindCallPointerFallbackSetupInput &,
    LowerInferenceSetupBootstrapState &stateInOut,
    std::string &errorOut) {
  if (!stateInOut.inferPointerTargetKind) {
    errorOut = "native backend missing inference expr-kind call-pointer fallback setup state: inferPointerTargetKind";
    return false;
  }

  stateInOut.inferCallExprPointerFallbackKind = [&stateInOut](const Expr &expr,
                                                              const LocalMap &localsIn,
                                                              LocalInfo::ValueKind &kindOut) {
    kindOut = LocalInfo::ValueKind::Unknown;
    LocalInfo::ValueKind pointerBuiltinKind = LocalInfo::ValueKind::Unknown;
    if (inferPointerBuiltinCallReturnKind(
            expr,
            localsIn,
            [&](const Expr &candidateExpr, const LocalMap &candidateLocals) {
              return stateInOut.inferPointerTargetKind(candidateExpr, candidateLocals);
            },
            pointerBuiltinKind) == PointerBuiltinCallReturnKindResolution::Resolved) {
      kindOut = pointerBuiltinKind;
      return true;
    }
    return false;
  };

  return true;
}

bool runLowerInferenceExprKindDispatchSetup(const LowerInferenceExprKindDispatchSetupInput &input,
                                            LowerInferenceSetupBootstrapState &stateInOut,
                                            std::string &errorOut) {
  if (input.error == nullptr) {
    errorOut = "native backend missing inference expr-kind dispatch setup dependency: error";
    return false;
  }
  if (!stateInOut.inferLiteralOrNameExprKind) {
    errorOut = "native backend missing inference expr-kind dispatch setup state: inferLiteralOrNameExprKind";
    return false;
  }
  if (!stateInOut.inferCallExprBaseKind) {
    errorOut = "native backend missing inference expr-kind dispatch setup state: inferCallExprBaseKind";
    return false;
  }
  if (!stateInOut.inferCallExprDirectReturnKind) {
    errorOut = "native backend missing inference expr-kind dispatch setup state: inferCallExprDirectReturnKind";
    return false;
  }
  if (!stateInOut.inferCallExprCountAccessGpuFallbackKind) {
    errorOut = "native backend missing inference expr-kind dispatch setup state: inferCallExprCountAccessGpuFallbackKind";
    return false;
  }
  if (!stateInOut.inferCallExprOperatorFallbackKind) {
    errorOut = "native backend missing inference expr-kind dispatch setup state: inferCallExprOperatorFallbackKind";
    return false;
  }
  if (!stateInOut.inferCallExprControlFlowFallbackKind) {
    errorOut = "native backend missing inference expr-kind dispatch setup state: inferCallExprControlFlowFallbackKind";
    return false;
  }
  if (!stateInOut.inferCallExprPointerFallbackKind) {
    errorOut = "native backend missing inference expr-kind dispatch setup state: inferCallExprPointerFallbackKind";
    return false;
  }

  std::string *const inferenceError = input.error;
  stateInOut.inferExprKind = [inferenceError, &stateInOut](const Expr &expr,
                                                            const LocalMap &localsIn) -> LocalInfo::ValueKind {
    LocalInfo::ValueKind literalOrNameKind = LocalInfo::ValueKind::Unknown;
    if (stateInOut.inferLiteralOrNameExprKind(expr, localsIn, literalOrNameKind)) {
      return literalOrNameKind;
    }
    switch (expr.kind) {
      case Expr::Kind::Call: {
        auto isBuiltinCountLikeCall = [](const Expr &candidate) {
          if (candidate.kind != Expr::Kind::Call) {
            return false;
          }
          if (isSimpleCallName(candidate, "count")) {
            return true;
          }
          std::string collectionName;
          std::string helperName;
          return getNamespacedCollectionHelperName(candidate, collectionName, helperName) && helperName == "count";
        };

        LocalInfo::ValueKind callBaseKind = LocalInfo::ValueKind::Unknown;
        if (stateInOut.inferCallExprBaseKind(expr, localsIn, callBaseKind)) {
          return callBaseKind;
        }
        LocalInfo::ValueKind callReturnKind = LocalInfo::ValueKind::Unknown;
        const auto callReturnResolution = stateInOut.inferCallExprDirectReturnKind(expr, localsIn, callReturnKind);
        if (callReturnResolution == CallExpressionReturnKindResolution::Resolved) {
          return callReturnKind;
        }
        LocalInfo::ValueKind callFallbackKind = LocalInfo::ValueKind::Unknown;
        if (stateInOut.inferCallExprCountAccessGpuFallbackKind(expr, localsIn, callFallbackKind)) {
          return callFallbackKind;
        }
        if (isBuiltinCountLikeCall(expr) && expr.args.size() == 1 && expr.args.front().kind == Expr::Kind::Call) {
          std::string accessName;
          const Expr &accessExpr = expr.args.front();
          if (getBuiltinArrayAccessName(accessExpr, accessName) && accessExpr.args.size() == 2) {
            const Expr &accessTarget = accessExpr.args.front();
            if (accessTarget.kind == Expr::Kind::Name) {
              auto it = localsIn.find(accessTarget.name);
              if (it != localsIn.end() &&
                  ((it->second.kind == LocalInfo::Kind::Map) ||
                   (it->second.kind == LocalInfo::Kind::Reference && it->second.referenceToMap)) &&
                  it->second.mapValueKind == LocalInfo::ValueKind::String) {
                return LocalInfo::ValueKind::Int32;
              }
            } else if (accessTarget.kind == Expr::Kind::Call) {
              std::string collectionName;
              if (getBuiltinCollectionName(accessTarget, collectionName) && collectionName == "map" &&
                  accessTarget.templateArgs.size() == 2) {
                const std::string &valueType = accessTarget.templateArgs[1];
                if (valueType == "string" || valueType == "/string") {
                  return LocalInfo::ValueKind::Int32;
                }
              }
            }
          }
        }
        if (callReturnResolution == CallExpressionReturnKindResolution::MatchedButUnsupported) {
          return LocalInfo::ValueKind::Unknown;
        }
        LocalInfo::ValueKind operatorFallbackKind = LocalInfo::ValueKind::Unknown;
        if (stateInOut.inferCallExprOperatorFallbackKind(expr, localsIn, operatorFallbackKind)) {
          return operatorFallbackKind;
        }
        LocalInfo::ValueKind controlFlowKind = LocalInfo::ValueKind::Unknown;
        if (stateInOut.inferCallExprControlFlowFallbackKind(expr, localsIn, *inferenceError, controlFlowKind)) {
          return controlFlowKind;
        }
        LocalInfo::ValueKind pointerFallbackKind = LocalInfo::ValueKind::Unknown;
        if (stateInOut.inferCallExprPointerFallbackKind(expr, localsIn, pointerFallbackKind)) {
          return pointerFallbackKind;
        }
        return LocalInfo::ValueKind::Unknown;
      }
      default:
        return LocalInfo::ValueKind::Unknown;
    }
  };
  return true;
}

} // namespace primec::ir_lowerer
