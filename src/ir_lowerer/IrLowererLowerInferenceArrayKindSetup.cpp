#include "IrLowererLowerInferenceSetup.h"

#include "IrLowererSetupTypeHelpers.h"

namespace primec::ir_lowerer {

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
  const auto resolveMethodCallDefinitionNoProbeError =
      [&stateInOut, &errorOut](const Expr &candidate, const LocalMap &candidateLocals) -> const Definition * {
    const std::string priorError = errorOut;
    const Definition *resolved = stateInOut.resolveMethodCallDefinition(candidate, candidateLocals);
    errorOut = priorError;
    return resolved;
  };
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
                                      resolveMethodCallDefinitionNoProbeError,
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
                                                  resolveMethodCallDefinitionNoProbeError,
                                                  stateInOut.getReturnInfo,
                                                  true,
                                                  kindOut,
                                                  nullptr,
                                                  stateInOut.inferExprKind);
        },
        [&](const Expr &candidate, const LocalMap &candidateLocals, LocalInfo::ValueKind &kindOut) {
          return resolveMethodCallReturnKind(
              candidate,
              candidateLocals,
              resolveMethodCallDefinitionNoProbeError,
              [&](const Expr &callExpr) {
                return ir_lowerer::resolveDefinitionCall(callExpr, *defMap, resolveExprPath);
              },
              stateInOut.getReturnInfo,
              true,
              kindOut);
        });
  };
  return true;
}

} // namespace primec::ir_lowerer
