#include "IrLowererLowerInferenceSetup.h"

#include "IrLowererHelpers.h"
#include "IrLowererLowerInferenceBaseKindHelpers.h"
#include "IrLowererSetupTypeHelpers.h"

namespace primec::ir_lowerer {

namespace {

bool isVectorCollectionStructPath(const std::string &structPath) {
  return structPath == "/vector" ||
         structPath == "/std/collections/experimental_vector/Vector" ||
         structPath.rfind("/std/collections/experimental_vector/Vector__", 0) == 0;
}

bool isMapCollectionStructPath(const std::string &structPath) {
  return structPath == "/map" ||
         structPath == "/std/collections/experimental_map/Map" ||
         structPath.rfind("/std/collections/experimental_map/Map__", 0) == 0;
}

} // namespace

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
  if (!input.inferStructExprPath) {
    errorOut = "native backend missing inference expr-kind call-fallback setup dependency: inferStructExprPath";
    return false;
  }
  if (!input.resolveStructFieldSlot) {
    errorOut = "native backend missing inference expr-kind call-fallback setup dependency: resolveStructFieldSlot";
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
  const auto inferStructExprPath = input.inferStructExprPath;
  const auto resolveStructFieldSlot = input.resolveStructFieldSlot;
  const auto *defMap = input.defMap;
  const auto resolveExprPath = input.resolveExprPath;

  const auto resolveFieldAccessCollectionInfo =
      [inferStructExprPath, resolveStructFieldSlot](const Expr &candidate,
                                                    const LocalMap &candidateLocals,
                                                    std::string &structPathOut,
                                                    LocalInfo::ValueKind &valueKindOut) {
        structPathOut.clear();
        valueKindOut = LocalInfo::ValueKind::Unknown;
        if (candidate.kind != Expr::Kind::Call || !candidate.isFieldAccess || candidate.args.size() != 1) {
          return false;
        }
        const std::string receiverStruct = inferStructExprPath(candidate.args.front(), candidateLocals);
        if (receiverStruct.empty()) {
          return false;
        }
        StructSlotFieldInfo fieldInfo;
        if (!resolveStructFieldSlot(receiverStruct, candidate.name, fieldInfo)) {
          return false;
        }
        structPathOut = fieldInfo.structPath;
        valueKindOut = fieldInfo.valueKind;
        return !structPathOut.empty();
      };

  const auto resolveCallCollectionAccessValueKind =
      [defMap, resolveExprPath, resolveFieldAccessCollectionInfo, &stateInOut](
          const Expr &candidate, const LocalMap &candidateLocals, LocalInfo::ValueKind &kindOut) {
        kindOut = LocalInfo::ValueKind::Unknown;
        if (candidate.kind != Expr::Kind::Call) {
          return false;
        }

        std::string builtinAccessName;
        if (candidate.args.size() == 2 && getBuiltinArrayAccessName(candidate, builtinAccessName)) {
          const Expr &receiver = candidate.args.front();
          auto assignReceiverCollectionValueKind = [&](const std::string &collectionName,
                                                      const std::vector<std::string> &collectionArgs) {
            if ((collectionName == "array" || collectionName == "vector" || collectionName == "soa_vector") &&
                collectionArgs.size() == 1) {
              kindOut = valueKindFromTypeName(collectionArgs.front());
              return kindOut != LocalInfo::ValueKind::Unknown;
            }
            if (collectionName == "map" && collectionArgs.size() == 2) {
              kindOut = valueKindFromTypeName(collectionArgs.back());
              return kindOut != LocalInfo::ValueKind::Unknown;
            }
            if (collectionName == "string") {
              kindOut = LocalInfo::ValueKind::Int32;
              return true;
            }
            return false;
          };
          auto assignArgsPackReceiverValueKind = [&](const Expr &receiverExpr) {
            auto assignFromLocal = [&](const LocalInfo &info, bool dereferenced) {
              if (!info.isArgsPack) {
                return false;
              }
              if (info.argsPackElementKind == LocalInfo::Kind::Array ||
                  info.argsPackElementKind == LocalInfo::Kind::Vector) {
                kindOut = info.valueKind;
                return kindOut != LocalInfo::ValueKind::Unknown;
              }
              if (info.argsPackElementKind == LocalInfo::Kind::Map) {
                kindOut = info.mapValueKind;
                return kindOut != LocalInfo::ValueKind::Unknown;
              }
              if ((info.argsPackElementKind == LocalInfo::Kind::Reference ||
                   info.argsPackElementKind == LocalInfo::Kind::Pointer) &&
                  (dereferenced || info.structTypeName.empty())) {
                if (info.referenceToArray || info.pointerToArray || info.referenceToVector || info.pointerToVector) {
                  kindOut = info.valueKind;
                  return kindOut != LocalInfo::ValueKind::Unknown;
                }
                if (info.referenceToMap || info.pointerToMap) {
                  kindOut = info.mapValueKind;
                  return kindOut != LocalInfo::ValueKind::Unknown;
                }
              }
              return false;
            };

            if (receiverExpr.kind == Expr::Kind::Name) {
              auto it = candidateLocals.find(receiverExpr.name);
              return it != candidateLocals.end() && assignFromLocal(it->second, false);
            }

            if (receiverExpr.kind == Expr::Kind::Call && isSimpleCallName(receiverExpr, "dereference") &&
                receiverExpr.args.size() == 1) {
              const Expr &derefTarget = receiverExpr.args.front();
              std::string accessName;
              if (derefTarget.kind == Expr::Kind::Call && getBuiltinArrayAccessName(derefTarget, accessName) &&
                  derefTarget.args.size() == 2 && derefTarget.args.front().kind == Expr::Kind::Name) {
                auto it = candidateLocals.find(derefTarget.args.front().name);
                return it != candidateLocals.end() && assignFromLocal(it->second, true);
              }
              return false;
            }

            std::string accessName;
            if (receiverExpr.kind == Expr::Kind::Call && getBuiltinArrayAccessName(receiverExpr, accessName) &&
                receiverExpr.args.size() == 2 && receiverExpr.args.front().kind == Expr::Kind::Name) {
              auto it = candidateLocals.find(receiverExpr.args.front().name);
              return it != candidateLocals.end() && assignFromLocal(it->second, false);
            }

            return false;
          };

          if (assignArgsPackReceiverValueKind(receiver)) {
            return true;
          }

          if (receiver.kind == Expr::Kind::Call) {
            const auto mapTargetInfo = resolveMapAccessTargetInfo(receiver, candidateLocals);
            if (mapTargetInfo.isMapTarget && mapTargetInfo.mapValueKind != LocalInfo::ValueKind::Unknown) {
              kindOut = mapTargetInfo.mapValueKind;
              return true;
            }

            std::string collectionName;
            if (getBuiltinCollectionName(receiver, collectionName)) {
              if (assignReceiverCollectionValueKind(collectionName, receiver.templateArgs)) {
                return true;
              }
            }

            const Definition *receiverDef = nullptr;
            if (receiver.isMethodCall) {
              if (stateInOut.resolveMethodCallDefinition) {
                receiverDef = stateInOut.resolveMethodCallDefinition(receiver, candidateLocals);
              }
            } else if (defMap != nullptr && resolveExprPath) {
              const std::string path = resolveExprPath(receiver);
              auto defIt = defMap->find(path);
              if (defIt != defMap->end()) {
                receiverDef = defIt->second;
              }
            }
            if (receiverDef != nullptr) {
              std::vector<std::string> collectionArgs;
              if (inferDeclaredReturnCollection(*receiverDef, collectionName, collectionArgs) &&
                  assignReceiverCollectionValueKind(collectionName, collectionArgs)) {
                return true;
              }
            }
          }
        }

        std::string fieldStructPath;
        if (resolveFieldAccessCollectionInfo(candidate, candidateLocals, fieldStructPath, kindOut) &&
            isVectorCollectionStructPath(fieldStructPath)) {
          return true;
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
        if (!inferDeclaredReturnCollection(*callee, collectionName, collectionArgs)) {
          return false;
        }
        if ((collectionName == "array" || collectionName == "vector" || collectionName == "soa_vector") &&
            collectionArgs.size() == 1) {
          kindOut = valueKindFromTypeName(collectionArgs.front());
          return true;
        }
        if (collectionName == "map" && collectionArgs.size() == 2) {
          kindOut = valueKindFromTypeName(collectionArgs.back());
          return true;
        }
        return false;
      };

  stateInOut.inferCallExprCountAccessGpuFallbackKind =
      [isArrayCountCall,
       isStringCountCall,
       isVectorCapacityCall,
       isEntryArgsName,
       resolveCallCollectionAccessValueKind,
       resolveFieldAccessCollectionInfo,
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
                    return resolveCallCollectionAccessValueKind(candidate, candidateLocals, candidateKindOut);
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
                  std::string collectionName;
                  std::string helperName;
                  const bool isCountCall =
                      candidateExpr.kind == Expr::Kind::Call && candidateExpr.args.size() == 1 &&
                      (isSimpleCallName(candidateExpr, "count") ||
                       (getNamespacedCollectionHelperName(candidateExpr, collectionName, helperName) &&
                        helperName == "count" && (collectionName == "vector" || collectionName == "map")));
                  if (isCountCall) {
                    std::string fieldStructPath;
                    LocalInfo::ValueKind fieldValueKind = LocalInfo::ValueKind::Unknown;
                    if (resolveFieldAccessCollectionInfo(
                            candidateExpr.args.front(), candidateLocals, fieldStructPath, fieldValueKind)) {
                      return isVectorCollectionStructPath(fieldStructPath) ||
                             isMapCollectionStructPath(fieldStructPath);
                    }
                    LocalInfo::ValueKind receiverCollectionKind = LocalInfo::ValueKind::Unknown;
                    if (resolveCallCollectionAccessValueKind(
                            candidateExpr.args.front(), candidateLocals, receiverCollectionKind) &&
                        receiverCollectionKind != LocalInfo::ValueKind::Unknown) {
                      return true;
                    }
                  }
                  return isArrayCountCall(candidateExpr, candidateLocals);
                },
                [&](const Expr &candidateExpr, const LocalMap &candidateLocals) {
                  return isStringCountCall(candidateExpr, candidateLocals);
                },
                [&](const Expr &candidateExpr, const LocalMap &candidateLocals) {
                  std::string collectionName;
                  std::string helperName;
                  const bool isCapacityCall =
                      candidateExpr.kind == Expr::Kind::Call && candidateExpr.args.size() == 1 &&
                      (isSimpleCallName(candidateExpr, "capacity") ||
                       (getNamespacedCollectionHelperName(candidateExpr, collectionName, helperName) &&
                        collectionName == "vector" && helperName == "capacity"));
                  if (isCapacityCall) {
                    std::string fieldStructPath;
                    LocalInfo::ValueKind fieldValueKind = LocalInfo::ValueKind::Unknown;
                    if (resolveFieldAccessCollectionInfo(
                            candidateExpr.args.front(), candidateLocals, fieldStructPath, fieldValueKind)) {
                      return isVectorCollectionStructPath(fieldStructPath);
                    }
                  }
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
                  return resolveCallCollectionAccessValueKind(candidate, candidateLocals, candidateKindOut);
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
                                                     const Expr &expr,
                                                     const LocalMap &localsIn,
                                                     LocalInfo::ValueKind &kindOut) {
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
  const auto *semanticProductTargets = input.semanticProductTargets;

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
                                                     semanticProductTargets,
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
              },
              [&](const Expr &candidateExpr) {
                return stateInOut.resolveDefinitionCall
                           ? stateInOut.resolveDefinitionCall(candidateExpr)
                           : nullptr;
              },
              semanticProductTargets);
        },
        [&](const std::string &path) { return defMap->find(path) != defMap->end(); },
        errorInOut,
        kindOut);
    return resolution == ControlFlowCallReturnKindResolution::Resolved;
  };

  return true;
}

bool runLowerInferenceExprKindCallPointerFallbackSetup(const LowerInferenceExprKindCallPointerFallbackSetupInput &,
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

} // namespace primec::ir_lowerer
