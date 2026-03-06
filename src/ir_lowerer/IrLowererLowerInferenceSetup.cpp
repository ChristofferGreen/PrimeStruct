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
        kindOut = it->second.referenceToArray ? LocalInfo::ValueKind::Unknown : it->second.valueKind;
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
        if (expr.name == "write" || expr.name == "write_line" || expr.name == "write_byte" ||
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
          if (arg.name == "write" || arg.name == "write_line" || arg.name == "write_byte" ||
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
          if (!resolveStructArrayInfoFromPath(structPath, structInfo)) {
            return false;
          }
          kindOut = structInfo.elementKind;
          return true;
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
                                                  kindOut);
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
  if (!stateInOut.getReturnInfo) {
    errorOut = "native backend missing inference expr-kind call-return setup state: getReturnInfo";
    return false;
  }

  const auto *defMap = input.defMap;
  const auto resolveExprPath = input.resolveExprPath;
  const auto isArrayCountCall = input.isArrayCountCall;
  const auto isStringCountCall = input.isStringCountCall;

  stateInOut.inferCallExprDirectReturnKind =
      [defMap, resolveExprPath, isArrayCountCall, isStringCountCall, &stateInOut](
          const Expr &expr, const LocalMap &localsIn, LocalInfo::ValueKind &kindOut) {
        return resolveCallExpressionReturnKind(
            expr,
            localsIn,
            [&](const Expr &candidate, const LocalMap &, LocalInfo::ValueKind &candidateKindOut, bool &matchedOut) {
              bool definitionMatched = false;
              const bool resolved = resolveDefinitionCallReturnKind(
                  candidate, *defMap, resolveExprPath, stateInOut.getReturnInfo, false, candidateKindOut, &definitionMatched);
              matchedOut = definitionMatched;
              return resolved;
            },
            [&](const Expr &candidate,
                const LocalMap &candidateLocals,
                LocalInfo::ValueKind &candidateKindOut,
                bool &matchedOut) {
              bool countMethodResolved = false;
              const bool resolved = resolveCountMethodCallReturnKind(candidate,
                                                                    candidateLocals,
                                                                    isArrayCountCall,
                                                                    isStringCountCall,
                                                                    stateInOut.resolveMethodCallDefinition,
                                                                    stateInOut.getReturnInfo,
                                                                    false,
                                                                    candidateKindOut,
                                                                    &countMethodResolved);
              matchedOut = countMethodResolved;
              return resolved;
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

  stateInOut.inferCallExprCountAccessGpuFallbackKind =
      [isArrayCountCall, isStringCountCall, isVectorCapacityCall, isEntryArgsName, &stateInOut](
          const Expr &expr, const LocalMap &localsIn, LocalInfo::ValueKind &kindOut) {
        kindOut = LocalInfo::ValueKind::Unknown;

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
                accessElementKind) == ArrayMapAccessElementKindResolution::Resolved) {
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

} // namespace primec::ir_lowerer
