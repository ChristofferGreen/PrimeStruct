#include "SemanticsValidator.h"
#include "StdlibCollectionSurfaceHelpers.h"
#include "SemanticsValidatorInferCollectionCompatibilityInternal.h"
#include "primec/ir/StdlibCollectionPaths.h"
#include "primec/frontend/StringLiteral.h"
#include "primec/support/CollectionHelperNames.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <functional>
#include <iomanip>
#include <optional>
#include <sstream>
#include <unordered_set>
#include "SemanticsValidatorExprCallContext.h"

namespace primec::semantics {

// The rules that find the callee of a call and validate the call against it.

bool SemanticsValidator::matchesResolvedFamilyPath([[maybe_unused]] ExprCallContext &cc,
                                                   std::string_view candidate,
                                                   std::string_view familyPath) {

  const std::string templatedPrefix = std::string(familyPath) + "<";
  const std::string specializedPrefix = std::string(familyPath) + "__t";
  const std::string overloadPrefix = std::string(familyPath) + "__ov";
  return candidate == familyPath || candidate.rfind(templatedPrefix, 0) == 0 ||
         candidate.rfind(specializedPrefix, 0) == 0 || candidate.rfind(overloadPrefix, 0) == 0;
}

PhaseStatus SemanticsValidator::resolveCallee(ExprCallContext &cc) {
  const std::vector<ParameterInfo> &params = cc.params;
  const std::unordered_map<std::string, BindingInfo> &locals = cc.locals;
  const Expr &expr = cc.expr;
  [[maybe_unused]] auto &dispatchBootstrap = cc.dispatchBootstrap;
  [[maybe_unused]] auto &shouldBuiltinValidateBareKeyValueContainsCall =
      cc.shouldBuiltinValidateBareKeyValueContainsCall;
  [[maybe_unused]] auto &shouldBuiltinValidateBareKeyValueAccessCall =
      cc.shouldBuiltinValidateBareKeyValueAccessCall;
  [[maybe_unused]] auto &resolved = cc.resolved;
  [[maybe_unused]] auto &resolvedMethod = cc.resolvedMethod;
  [[maybe_unused]] auto &collectionDispatchSetup = cc.collectionDispatchSetup;
  [[maybe_unused]] auto &resolvedDefinition = cc.resolvedDefinition;
  [[maybe_unused]] auto &calleeParamsIt = cc.calleeParamsIt;
  [[maybe_unused]] auto publishExprRootDiagnostic = [&]() -> bool {
    captureExprContext(expr);
    return publishCurrentStructuredDiagnosticNow();
  };
  [[maybe_unused]] auto failExprRootDiagnostic = [&](std::string message) -> bool {
    return failExprDiagnostic(expr, std::move(message));
  };
  std::string activeSpecializationSuffix;
  if (currentDefinitionContext_ != nullptr) {
    const size_t suffixPos = currentDefinitionContext_->fullPath.find("__t");
    if (suffixPos != std::string::npos) {
      activeSpecializationSuffix = currentDefinitionContext_->fullPath.substr(suffixPos);
    }
  }
  auto matchesActiveSpecialization = [&](std::string_view candidate) {
    return !activeSpecializationSuffix.empty() &&
           candidate.size() >= activeSpecializationSuffix.size() &&
           candidate.compare(candidate.size() - activeSpecializationSuffix.size(),
                             activeSpecializationSuffix.size(),
                             activeSpecializationSuffix) == 0;
  };
  auto it = defMap_.find(resolved);
  cc.resolvedDefinition = it != defMap_.end() ? it->second : nullptr;
  if (resolvedDefinition == nullptr && hasDefinitionFamilyPath(resolved)) {
    const Definition *firstMatch = nullptr;
    for (const auto &def : program_.definitions) {
      if (!matchesResolvedFamilyPath(cc, def.fullPath, resolved)) {
        continue;
      }
      if (firstMatch == nullptr) {
        firstMatch = &def;
      }
      if (matchesActiveSpecialization(def.fullPath)) {
        resolvedDefinition = &def;
        break;
      }
    }
    if (resolvedDefinition == nullptr) {
      resolvedDefinition = firstMatch;
    }
  }
  cc.calleeParamsIt = paramsByDef_.find(resolved);
  if (calleeParamsIt == paramsByDef_.end() && hasDefinitionFamilyPath(resolved)) {
    auto firstMatchIt = paramsByDef_.end();
    for (auto candidateIt = paramsByDef_.begin(); candidateIt != paramsByDef_.end();
         ++candidateIt) {
      if (!matchesResolvedFamilyPath(cc, candidateIt->first, resolved)) {
        continue;
      }
      if (firstMatchIt == paramsByDef_.end()) {
        firstMatchIt = candidateIt;
      }
      if (matchesActiveSpecialization(candidateIt->first)) {
        calleeParamsIt = candidateIt;
        break;
      }
    }
    if (calleeParamsIt == paramsByDef_.end()) {
      calleeParamsIt = firstMatchIt;
    }
  }
  // A vector access (`values[i]`, `at`, `at_unsafe`) of a builtin vector, outside a specialization of the helper,
  // that found only a specialization for another element type (such as `at<Route>`, made by /std/http, for a
  // `vector<i32>`): the late builtin fallback validates it, as when no specialization exists, rather than the
  // other element type's parameters.
  auto elementTypeLeaf = [](std::string type) {
    type.erase(std::remove_if(
                   type.begin(), type.end(), [](unsigned char c) { return std::isspace(c) != 0; }),
               type.end());
    const size_t slash = type.find_last_of('/');
    return slash == std::string::npos ? type : type.substr(slash + 1);
  };
  if (it == defMap_.end() && resolvedDefinition != nullptr &&
      calleeParamsIt != paramsByDef_.end() && !calleeParamsIt->second.empty() &&
      !matchesActiveSpecialization(resolvedDefinition->fullPath) &&
      (isStdNamespacedVectorCompatibilityHelperPath(resolved, "at") ||
       isStdNamespacedVectorCompatibilityHelperPath(resolved, "at_unsafe")) &&
      !expr.args.empty() && dispatchBootstrap.dispatchResolvers.resolveVectorTarget) {
    std::string receiverElemType;
    std::string candidateElemType;
    if (dispatchBootstrap.dispatchResolvers.resolveVectorTarget(expr.args.front(),
                                                                receiverElemType) &&
        extractCollectionVectorElementType(calleeParamsIt->second.front().binding,
                                           candidateElemType) &&
        !receiverElemType.empty() && !candidateElemType.empty() &&
        elementTypeLeaf(receiverElemType) != elementTypeLeaf(candidateElemType)) {
      resolvedDefinition = nullptr;
      calleeParamsIt = paramsByDef_.end();
    }
  }
  if (!expr.isMethodCall && isResolvedKeyValueConstructorPath(resolved) &&
      expr.templateArgs.size() == 2) {
    std::string keyError;
    if (!validateBuiltinComparableKeyType(expr.templateArgs.front(), nullptr, keyError)) {
      return cc.done(failExprRootDiagnostic(std::move(keyError)));
    }
    for (const Expr &arg : expr.args) {
      if (!validateExpr(params, locals, arg)) {
        return cc.done(false);
      }
    }
    for (std::size_t i = 0; i < expr.args.size(); i += 2) {
      if (i + 1 >= expr.args.size()) {
        if (!validateExpr(params, locals, expr.args[i])) {
          return cc.done(false);
        }
        break;
      }
      if (!this->validateCollectionElementType(expr.args[i],
                                               expr.templateArgs[0],
                                               "map constructor requires key type ",
                                               params,
                                               locals,
                                               dispatchBootstrap.dispatchResolvers)) {
        return cc.done(false);
      }
      if (!this->validateCollectionElementType(expr.args[i + 1],
                                               expr.templateArgs[1],
                                               "map constructor requires value type ",
                                               params,
                                               locals,
                                               dispatchBootstrap.dispatchResolvers)) {
        return cc.done(false);
      }
    }
  }
  ExprLateBuiltinContext lateBuiltinContext;
  prepareExprLateBuiltinContext(params,
                                locals,
                                dispatchBootstrap.dispatchResolverAdapters,
                                dispatchBootstrap.dispatchResolvers,
                                lateBuiltinContext);
  bool handledLateBuiltin = false;
  if (!validateExprLateBuiltins(
          params, locals, expr, resolved, resolvedMethod, lateBuiltinContext, handledLateBuiltin)) {
    return cc.done(false);
  }
  if (handledLateBuiltin) {
    return cc.done(true);
  }
  ExprCountCapacityBuiltinContext countCapacityBuiltinContext;
  prepareExprCountCapacityBuiltinContext(dispatchBootstrap.dispatchResolvers,
                                         countCapacityBuiltinContext);
  bool handledCountCapacityBuiltin = false;
  if (!validateExprCountCapacityBuiltins(params,
                                         locals,
                                         expr,
                                         resolved,
                                         resolvedMethod,
                                         countCapacityBuiltinContext,
                                         handledCountCapacityBuiltin)) {
    return cc.done(false);
  }
  if (handledCountCapacityBuiltin) {
    return cc.done(true);
  }
  const bool resolvedUsesCanonicalSoaNamespace =
      resolved.rfind(collection_paths::modulePrefix(collection_paths::kLegacySoaVectorFolder), 0) ==
      0;
  std::string resolvedWithoutSpecialization = resolved;
  if (const size_t suffix = resolvedWithoutSpecialization.find("__"); suffix != std::string::npos) {
    resolvedWithoutSpecialization.erase(suffix);
  }
  const std::string resolvedSoaGetCanonical =
      canonicalizeLegacySoaGetHelperPath(resolvedWithoutSpecialization);
  const std::string resolvedSoaRefCanonical =
      canonicalizeLegacySoaRefHelperPath(resolvedWithoutSpecialization);
  const std::string resolvedSoaToAosCanonical =
      canonicalizeLegacySoaToAosHelperPath(resolvedWithoutSpecialization);
  const bool resolvedIsSoaAccess =
      isLegacyOrCanonicalSoaHelperPath(resolvedSoaGetCanonical, "get") ||
      isLegacyOrCanonicalSoaHelperPath(resolvedSoaGetCanonical, collection_helpers::kGetRef) ||
      isCanonicalSoaRefLikeHelperPath(resolvedSoaRefCanonical) ||
      isExperimentalSoaGetLikeHelperPath(resolvedWithoutSpecialization) ||
      isExperimentalSoaRefLikeHelperPath(resolvedWithoutSpecialization);
  const bool resolvedIsSoaConversion =
      resolvedWithoutSpecialization == "/to_soa" ||
      isLegacyOrCanonicalSoaHelperPath(resolvedSoaToAosCanonical, "to_aos") ||
      isLegacyOrCanonicalSoaHelperPath(resolvedSoaToAosCanonical, collection_helpers::kToAosRef) ||
      isExperimentalSoaVectorConversionFamilyPath(resolvedWithoutSpecialization);
  const bool shouldLateValidateDirectSoaSurface =
      ((isSimpleCallName(expr, "get") || isSimpleCallName(expr, collection_helpers::kGetRef) ||
        isSimpleCallName(expr, "ref") || isSimpleCallName(expr, collection_helpers::kRefRef)) &&
       resolvedIsSoaAccess) ||
      ((isSimpleCallName(expr, "to_soa") || isSimpleCallName(expr, "to_aos") ||
        isSimpleCallName(expr, collection_helpers::kToAosRef)) &&
       resolvedIsSoaConversion);
  const bool shouldLateValidateCanonicalSoaToAos =
      resolvedUsesCanonicalSoaNamespace && isCanonicalStdlibSoaHelperPath(resolved, "to_aos");
  const bool shouldLateValidateCanonicalSoaToAosRef =
      resolvedUsesCanonicalSoaNamespace &&
      isCanonicalStdlibSoaHelperPath(resolved, collection_helpers::kToAosRef);
  if (resolvedDefinition == nullptr || resolvedMethod || shouldLateValidateDirectSoaSurface ||
      shouldLateValidateCanonicalSoaToAos || shouldLateValidateCanonicalSoaToAosRef) {
    ExprLateMapSoaBuiltinContext lateMapSoaBuiltinContext;
    prepareExprLateMapSoaBuiltinContext(shouldBuiltinValidateBareKeyValueContainsCall,
                                        dispatchBootstrap.dispatchResolvers,
                                        lateMapSoaBuiltinContext);
    bool handledKeyValueSoaBuiltin = false;
    if (!validateExprLateMapSoaBuiltins(params,
                                        locals,
                                        expr,
                                        resolved,
                                        resolvedMethod,
                                        lateMapSoaBuiltinContext,
                                        handledKeyValueSoaBuiltin)) {
      return cc.done(false);
    }
    if (handledKeyValueSoaBuiltin) {
      return cc.done(true);
    }
    ExprLateFallbackBuiltinContext lateFallbackBuiltinContext;
    prepareExprLateFallbackBuiltinContext(
        collectionDispatchSetup.isStdNamespacedVectorAccessCall,
        collectionDispatchSetup.shouldAllowStdAccessCompatibilityFallback,
        collectionDispatchSetup.hasStdNamespacedVectorAccessDefinition,
        collectionDispatchSetup.isStdNamespacedMapAccessCall,
        collectionDispatchSetup.hasStdNamespacedKeyValueAccessDefinition,
        shouldBuiltinValidateBareKeyValueAccessCall,
        dispatchBootstrap.dispatchResolvers,
        lateFallbackBuiltinContext);
    bool handledLateFallbackBuiltin = false;
    if (!validateExprLateFallbackBuiltins(params,
                                          locals,
                                          expr,
                                          resolved,
                                          resolvedMethod,
                                          lateFallbackBuiltinContext,
                                          handledLateFallbackBuiltin)) {
      return cc.done(false);
    }
    if (handledLateFallbackBuiltin) {
      return cc.done(true);
    }
    bool handledMutationBorrowBuiltin = false;
    if (!validateExprMutationBorrowBuiltins(params, locals, expr, handledMutationBorrowBuiltin)) {
      return cc.done(false);
    }
    if (handledMutationBorrowBuiltin) {
      return cc.done(true);
    }
    ExprLateCallCompatibilityContext lateCallCompatibilityContext;
    prepareExprLateCallCompatibilityContext(dispatchBootstrap.dispatchResolvers,
                                            lateCallCompatibilityContext);
    bool handledLateCallCompatibility = false;
    if (!validateExprLateCallCompatibility(params,
                                           locals,
                                           expr,
                                           resolved,
                                           lateCallCompatibilityContext,
                                           handledLateCallCompatibility)) {
      return cc.done(false);
    }
    if (handledLateCallCompatibility) {
      return cc.done(true);
    }
    ExprLateUnknownTargetFallbackContext lateUnknownTargetFallbackContext;
    lateUnknownTargetFallbackContext.resolveMapTarget = dispatchBootstrap.resolveMapTarget;
    lateUnknownTargetFallbackContext.isIndexedArgsPackKeyValueReceiverTarget =
        [&](const Expr &target) {
          return this->isIndexedArgsPackKeyValueReceiverTarget(target,
                                                               dispatchBootstrap.dispatchResolvers);
        };
    bool handledLateUnknownTargetFallback = false;
    if (!validateExprLateUnknownTargetFallbacks(params,
                                                locals,
                                                expr,
                                                lateUnknownTargetFallbackContext,
                                                handledLateUnknownTargetFallback)) {
      if (error_.empty()) {
        return cc.done(failExprRootDiagnostic("validateExprLateUnknownTargetFallbacks failed"));
      }
      return cc.done(false);
    }
    if (handledLateUnknownTargetFallback) {
      return cc.done(true);
    }
  }
  return PhaseStatus::Continue;
}

PhaseStatus SemanticsValidator::validateResolvedCall(ExprCallContext &cc) {
  const std::vector<ParameterInfo> &params = cc.params;
  const std::unordered_map<std::string, BindingInfo> &locals = cc.locals;
  const Expr &expr = cc.expr;
  [[maybe_unused]] auto &dispatchBootstrap = cc.dispatchBootstrap;
  [[maybe_unused]] auto &resolved = cc.resolved;
  [[maybe_unused]] auto &hasMethodReceiverIndex = cc.hasMethodReceiverIndex;
  [[maybe_unused]] auto &methodReceiverIndex = cc.methodReceiverIndex;
  [[maybe_unused]] auto &resolvedDefinition = cc.resolvedDefinition;
  [[maybe_unused]] auto &calleeParamsIt = cc.calleeParamsIt;
  [[maybe_unused]] auto &expressionIsStatementContext = cc.expressionIsStatementContext;
  [[maybe_unused]] auto publishExprRootDiagnostic = [&]() -> bool {
    captureExprContext(expr);
    return publishCurrentStructuredDiagnosticNow();
  };
  [[maybe_unused]] auto failExprRootDiagnostic = [&](std::string message) -> bool {
    return failExprDiagnostic(expr, std::move(message));
  };
  if (!expr.isMethodCall && isResolvedKeyValueConstructorPath(resolved) &&
      !(hasNamedArguments(expr.argNames) &&
        (defMap_.count(resolved) > 0 || hasImportedDefinitionPath(resolved)))) {
    if (hasNamedArguments(expr.argNames)) {
      return cc.done(failExprRootDiagnostic("named arguments not supported for builtin calls"));
    }
    if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
      return cc.done(
          failExprRootDiagnostic("block arguments require a definition target: " + resolved));
    }
    if (expr.templateArgs.size() == 2) {
      std::string keyError;
      if (!validateBuiltinComparableKeyType(expr.templateArgs.front(), nullptr, keyError)) {
        return cc.done(failExprRootDiagnostic(std::move(keyError)));
      }
    }
    for (const Expr &arg : expr.args) {
      if (!validateExpr(params, locals, arg)) {
        return cc.done(false);
      }
    }
    if (expr.templateArgs.size() == 2 && !expr.args.empty()) {
      const Definition *currentDef = nullptr;
      if (!currentValidationState_.context.definitionPath.empty()) {
        auto currentDefIt = defMap_.find(currentValidationState_.context.definitionPath);
        if (currentDefIt != defMap_.end()) {
          currentDef = currentDefIt->second;
        }
      }
      const std::vector<std::string> *definitionTemplateArgs =
          currentDef == nullptr ? nullptr : &currentDef->templateArgs;
      std::string definitionNamespacePrefix = expr.namespacePrefix;
      if (currentDef != nullptr && definitionNamespacePrefix.empty()) {
        definitionNamespacePrefix = currentDef->namespacePrefix;
      }
      std::unordered_set<std::string> visitingStructs;
      const std::string &valueType = expr.templateArgs[1];
      if (!isRelocationTrivialContainerElementType(
              valueType, definitionNamespacePrefix, definitionTemplateArgs, visitingStructs)) {
        return cc.done(failExprRootDiagnostic(
            std::string("map ") +
            "literal requires relocation-trivial map value type until container "
            "move/reallocation semantics are implemented: " +
            valueType));
      }
      for (std::size_t i = 0; i < expr.args.size(); i += 2) {
        if (i + 1 >= expr.args.size()) {
          if (!validateExpr(params, locals, expr.args[i])) {
            return cc.done(false);
          }
          break;
        }
        if (!this->validateCollectionElementType(expr.args[i],
                                                 expr.templateArgs[0],
                                                 "map constructor requires key type ",
                                                 params,
                                                 locals,
                                                 dispatchBootstrap.dispatchResolvers)) {
          return cc.done(false);
        }
        if (!this->validateCollectionElementType(expr.args[i + 1],
                                                 expr.templateArgs[1],
                                                 "map constructor requires value type ",
                                                 params,
                                                 locals,
                                                 dispatchBootstrap.dispatchResolvers)) {
          return cc.done(false);
        }
      }
    }
    return cc.done(true);
  }
  if (resolvedDefinition == nullptr || calleeParamsIt == paramsByDef_.end()) {
    const std::string unknownCallTarget = formatUnknownCallTarget(expr);
    if (expressionIsStatementContext &&
        (isStdNamespacedVectorCompatibilityHelperPath(resolved, "push") ||
         isStdNamespacedVectorCompatibilityHelperPath(unknownCallTarget, "push") ||
         isLegacyOrCanonicalSoaHelperPath(resolved, "push") ||
         isLegacyOrCanonicalSoaHelperPath(unknownCallTarget, "push") ||
         isStdNamespacedVectorCompatibilityHelperPath(resolved, "pop") ||
         isStdNamespacedVectorCompatibilityHelperPath(unknownCallTarget, "pop") ||
         isStdNamespacedVectorCompatibilityHelperPath(resolved, "reserve") ||
         isStdNamespacedVectorCompatibilityHelperPath(unknownCallTarget, "reserve") ||
         isLegacyOrCanonicalSoaHelperPath(resolved, "reserve") ||
         isLegacyOrCanonicalSoaHelperPath(unknownCallTarget, "reserve") ||
         isStdNamespacedVectorCompatibilityHelperPath(resolved, "clear") ||
         isStdNamespacedVectorCompatibilityHelperPath(unknownCallTarget, "clear") ||
         isStdNamespacedVectorCompatibilityHelperPath(resolved, "remove_at") ||
         isStdNamespacedVectorCompatibilityHelperPath(unknownCallTarget, "remove_at") ||
         isStdNamespacedVectorCompatibilityHelperPath(resolved, "remove_swap") ||
         isStdNamespacedVectorCompatibilityHelperPath(unknownCallTarget, "remove_swap"))) {
      for (const Expr &arg : expr.args) {
        if (!validateExpr(params, locals, arg)) {
          return cc.done(false);
        }
      }
      return cc.done(true);
    }
    if (isStdNamespacedVectorCompatibilityHelperPath(resolved, "count") && expr.args.size() != 1) {
      if (hasNamedArguments(expr.argNames)) {
        return cc.done(failExprRootDiagnostic("named arguments not supported for builtin calls"));
      }
      return cc.done(failExprRootDiagnostic("argument count mismatch for builtin count"));
    }
    if (isStdNamespacedVectorCompatibilityHelperPath(resolved, "capacity") &&
        expr.args.size() != 1) {
      if (hasNamedArguments(expr.argNames)) {
        return cc.done(failExprRootDiagnostic("named arguments not supported for builtin calls"));
      }
      return cc.done(failExprRootDiagnostic("argument count mismatch for builtin capacity"));
    }
    if (expr.isMethodCall && isStdNamespacedVectorCompatibilityHelperPath(resolved, "count") &&
        expr.args.size() != 1) {
      if (hasNamedArguments(expr.argNames)) {
        return cc.done(failExprRootDiagnostic("named arguments not supported for builtin calls"));
      }
      return cc.done(failExprRootDiagnostic("argument count mismatch for builtin count"));
    }
    if (expr.isMethodCall && isStdNamespacedVectorCompatibilityHelperPath(resolved, "capacity") &&
        expr.args.size() != 1) {
      if (hasNamedArguments(expr.argNames)) {
        return cc.done(failExprRootDiagnostic("named arguments not supported for builtin calls"));
      }
      return cc.done(failExprRootDiagnostic("argument count mismatch for builtin capacity"));
    }
    return cc.done(failExprRootDiagnostic("unknown call target: " + unknownCallTarget));
  }
  if (!validateCalleeEffects(expr, *resolvedDefinition)) {
    return cc.done(false);
  }
  const auto &calleeParams = calleeParamsIt->second;
  ExprResolvedCallSetup resolvedCallSetup;
  prepareExprResolvedCallSetup(params,
                               locals,
                               expr,
                               resolved,
                               dispatchBootstrap.dispatchResolvers,
                               *resolvedDefinition,
                               calleeParams,
                               hasMethodReceiverIndex,
                               methodReceiverIndex,
                               resolvedCallSetup);
  bool handledResolvedStructConstructor = false;
  if (!validateExprResolvedStructConstructorCall(params,
                                                 locals,
                                                 expr,
                                                 resolved,
                                                 resolvedCallSetup.resolvedStructConstructorContext,
                                                 handledResolvedStructConstructor)) {
    return cc.done(false);
  }
  if (handledResolvedStructConstructor) {
    return cc.done(true);
  }
  bool handledResolvedCallArguments = false;
  if (!validateExprResolvedCallArguments(params,
                                         locals,
                                         expr,
                                         resolved,
                                         resolvedCallSetup.resolvedCallArgumentContext,
                                         handledResolvedCallArguments)) {
    if (error_.empty()) {
      return cc.done(failExprRootDiagnostic("validateExprResolvedCallArguments failed"));
    }
    return cc.done(false);
  }
  if (handledResolvedCallArguments) {
    return cc.done(true);
  }
  return PhaseStatus::Continue;
}

} // namespace primec::semantics
