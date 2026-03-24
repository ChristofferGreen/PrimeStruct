#include "IrLowererLowerInferenceSetup.h"

#include "IrLowererSetupTypeHelpers.h"

namespace primec::ir_lowerer {

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
  const auto resolveMethodCallDefinitionNoProbeError =
      [&stateInOut, &errorOut](const Expr &candidate, const LocalMap &candidateLocals) -> const Definition * {
    const std::string priorError = errorOut;
    const Definition *resolved = stateInOut.resolveMethodCallDefinition(candidate, candidateLocals);
    errorOut = priorError;
    return resolved;
  };
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
       defMap,
       resolveExprPath,
       &stateInOut,
       isNamespacedCollectionReceiverProbeCall,
       resolveMethodCallDefinitionNoProbeError,
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
                                                   resolveMethodCallDefinitionNoProbeError,
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
                                                                                resolveMethodCallDefinitionNoProbeError,
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
                                                                resolveMethodCallDefinitionNoProbeError,
                                                                [&](const Expr &callExpr) {
                                                                  return ir_lowerer::resolveDefinitionCall(
                                                                      callExpr, *defMap, resolveExprPath);
                                                                },
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

} // namespace primec::ir_lowerer
