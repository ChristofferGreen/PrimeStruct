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
#include "SemanticsValidatorExprCallState.h"

namespace primec::semantics {

PhaseStatus SemanticsValidator::validateExprCallPhase6([[maybe_unused]] const std::vector<ParameterInfo> &params, [[maybe_unused]] const std::unordered_map<std::string, BindingInfo> &locals, [[maybe_unused]] const Expr &expr, ValidateExprCallState &st) {
  [[maybe_unused]] auto &enclosingStatements = st.enclosingStatements;
  [[maybe_unused]] auto &statementIndex = st.statementIndex;
  [[maybe_unused]] auto &expressionIsStatementContext = st.expressionIsStatementContext;
  [[maybe_unused]] auto publishExprRootDiagnostic = [&]() -> bool {
    captureExprContext(expr);
    return publishCurrentStructuredDiagnosticNow();
  };
  [[maybe_unused]] auto failExprRootDiagnostic = [&](std::string message) -> bool {
    return failExprDiagnostic(expr, std::move(message));
  };
  [[maybe_unused]] auto &hasVectorHelperCallResolution = st.hasVectorHelperCallResolution;
  [[maybe_unused]] auto &dispatchBootstrap = st.dispatchBootstrap;
  [[maybe_unused]] auto &shouldBuiltinValidateBareKeyValueContainsCall = st.shouldBuiltinValidateBareKeyValueContainsCall;
  [[maybe_unused]] auto &shouldBuiltinValidateBareKeyValueAccessCall = st.shouldBuiltinValidateBareKeyValueAccessCall;
  [[maybe_unused]] auto &resolved = st.resolved;
  [[maybe_unused]] auto &methodCompatibilitySetup = st.methodCompatibilitySetup;
  [[maybe_unused]] auto &resolvedMethod = st.resolvedMethod;
  [[maybe_unused]] auto &usedMethodTarget = st.usedMethodTarget;
  [[maybe_unused]] auto &hasMethodReceiverIndex = st.hasMethodReceiverIndex;
  [[maybe_unused]] auto &methodReceiverIndex = st.methodReceiverIndex;
  [[maybe_unused]] auto &collectionDispatchSetup = st.collectionDispatchSetup;
    ExprCollectionCountCapacityDispatchContext collectionCountCapacityDispatchContext;
    collectionCountCapacityDispatchContext.isNamespacedVectorHelperCall =
        collectionDispatchSetup.isNamespacedVectorHelperCall;
    collectionCountCapacityDispatchContext.namespacedHelper =
        collectionDispatchSetup.namespacedHelper;
    collectionCountCapacityDispatchContext.isNamespacedVectorCapacityCall =
        collectionDispatchSetup.isNamespacedVectorCapacityCall;
    collectionCountCapacityDispatchContext
        .isDirectStdNamespacedVectorCountWrapperKeyValueTarget =
        collectionDispatchSetup.isDirectStdNamespacedVectorCountWrapperKeyValueTarget;
    collectionCountCapacityDispatchContext.resolveMapTarget =
        dispatchBootstrap.resolveMapTarget;
    collectionCountCapacityDispatchContext
        .isArrayNamespacedVectorCountCompatibilityCall =
        [&](const Expr &target) {
          return this->isArrayNamespacedVectorCountCompatibilityCall(
              target, dispatchBootstrap.dispatchResolvers);
        };
    collectionCountCapacityDispatchContext.tryRewriteBareVectorHelperCall =
        [&](const std::string &helperName, Expr &rewrittenOut) {
          return this->tryRewriteBareVectorHelperCall(
              expr, helperName, dispatchBootstrap.dispatchResolvers,
              rewrittenOut);
        };
    collectionCountCapacityDispatchContext
        .promoteCapacityToBuiltinValidation =
        methodCompatibilitySetup.promoteCapacityToBuiltinValidation;
    collectionCountCapacityDispatchContext
        .isNonCollectionStructCapacityTarget =
        methodCompatibilitySetup.isNonCollectionStructCapacityTarget;
    bool handledCollectionCountCapacityTarget = false;
    std::optional<Expr> rewrittenCollectionCountCapacityCall;
    if (!resolveExprCollectionCountCapacityTarget(
            params, locals, expr, collectionCountCapacityDispatchContext,
            handledCollectionCountCapacityTarget,
            rewrittenCollectionCountCapacityCall, resolved, resolvedMethod,
            usedMethodTarget, hasMethodReceiverIndex, methodReceiverIndex)) {
      return st.done(false);
    }
    if (handledCollectionCountCapacityTarget &&
        rewrittenCollectionCountCapacityCall.has_value()) {
      return st.done(validateExpr(params, locals,
                          *rewrittenCollectionCountCapacityCall));
    }
    std::optional<Expr> rewrittenDirectCollectionFallbackCall;
    if (!validateExprDirectCollectionFallbacks(
            params,
            locals,
            expr,
            resolved,
            dispatchBootstrap.dispatchResolvers,
            rewrittenDirectCollectionFallbackCall)) {
      return st.done(false);
    }
    if (rewrittenDirectCollectionFallbackCall.has_value()) {
      return st.done(validateExpr(params, locals,
                          *rewrittenDirectCollectionFallbackCall));
    }
    ExprCollectionAccessDispatchContext collectionAccessDispatchContext;
    prepareExprCollectionAccessDispatchContext(
        collectionDispatchSetup,
        shouldBuiltinValidateBareKeyValueContainsCall,
        shouldBuiltinValidateBareKeyValueAccessCall,
        dispatchBootstrap.dispatchResolvers,
        dispatchBootstrap.resolveMapTarget,
        collectionAccessDispatchContext);
    bool handledCollectionAccessTarget = false;
    if (!resolveExprCollectionAccessTarget(
            params, locals, expr, collectionAccessDispatchContext,
            handledCollectionAccessTarget, resolved, resolvedMethod,
            usedMethodTarget, hasMethodReceiverIndex,
            methodReceiverIndex)) {
      return st.done(false);
    }
    bool handledPostAccessPrecheck = false;
    if (!validateExprPostAccessPrechecks(
            params,
            locals,
            expr,
            resolved,
            resolvedMethod,
            usedMethodTarget,
            dispatchBootstrap.dispatchResolverAdapters,
            enclosingStatements,
            statementIndex,
            handledPostAccessPrecheck)) {
      return st.done(false);
    }
    if (handledPostAccessPrecheck) {
      return st.done(true);
    }
    ExprNamedArgumentBuiltinContext namedArgumentBuiltinContext;
    prepareExprNamedArgumentBuiltinContext(
        hasVectorHelperCallResolution,
        dispatchBootstrap.dispatchResolvers,
        namedArgumentBuiltinContext);
    if (!validateExprNamedArguments(params, locals, expr, resolved,
                                    resolvedMethod,
                                    namedArgumentBuiltinContext)) {
      if (error_.empty()) {
        return st.done(failExprRootDiagnostic("validateExprNamedArguments failed"));
      }
      return st.done(false);
    }
    auto validateResolvedCanonicalKeyValueAccessKey = [&]() -> bool {
      if (expr.isMethodCall || expr.args.size() != 2) {
        return true;
      }
      const StdlibSurfaceMetadata *metadata =
          keyValueHelperSurfaceMetadataLocal();
      std::string helperName;
      if (metadata == nullptr ||
          !resolvePublishedCollectionHelperResolvedPath(resolved,
                                                        metadata->id,
                                                        helperName)) {
        return true;
      }
      if (!collection_helpers::isTryAtHelperName(helperName) &&
          !collection_helpers::isAtHelperName(helperName) &&
          !collection_helpers::isAtUnsafeHelperName(helperName)) {
        return true;
      }
      std::string keyValueKeyType;
      if (!resolveKeyValueKeyType(expr.args.front(),
                                  dispatchBootstrap.dispatchResolvers,
                                  keyValueKeyType)) {
        std::string receiverTypeText;
        std::string keyValueValueType;
        if (!inferQueryExprTypeText(expr.args.front(), params, locals,
                                    receiverTypeText) ||
            !extractKeyValueCollectionTypesFromTypeText(receiverTypeText,
                                                        keyValueKeyType,
                                                        keyValueValueType)) {
          return true;
        }
      }
      auto failKeyValueKeyDiagnostic = [&]() {
        if (collection_helpers::isTryAtHelperName(helperName)) {
          if (normalizeBindingTypeName(keyValueKeyType) == "string") {
            return failExprDiagnostic(expr.args[1],
                                      "tryAt requires string map key");
          }
          return failExprDiagnostic(expr.args[1],
                                    "tryAt requires map key type " +
                                        keyValueKeyType);
        }
        return failExprDiagnostic(
            expr.args[1],
            "argument type mismatch for " + resolved + " parameter key");
      };
      if (normalizeBindingTypeName(keyValueKeyType) == "string") {
        if (!isStringExprForArgumentValidation(
                expr.args[1], dispatchBootstrap.dispatchResolvers)) {
          return failKeyValueKeyDiagnostic();
        }
        return true;
      }
      const ReturnKind keyKind =
          returnKindForTypeName(normalizeBindingTypeName(keyValueKeyType));
      if (keyKind == ReturnKind::Unknown) {
        return true;
      }
      if (dispatchBootstrap.dispatchResolvers.resolveStringTarget != nullptr &&
          dispatchBootstrap.dispatchResolvers.resolveStringTarget(expr.args[1])) {
        return failKeyValueKeyDiagnostic();
      }
      const ReturnKind argKind = inferExprReturnKind(expr.args[1], params, locals);
      if (argKind != ReturnKind::Unknown && argKind != keyKind) {
        return failKeyValueKeyDiagnostic();
      }
      return true;
    };
    if (!validateResolvedCanonicalKeyValueAccessKey()) {
      return st.done(false);
    }
  st.matchesResolvedFamilyPath = [&](std::string_view candidate,
                                        std::string_view familyPath) {
      const std::string templatedPrefix = std::string(familyPath) + "<";
      const std::string specializedPrefix = std::string(familyPath) + "__t";
      const std::string overloadPrefix = std::string(familyPath) + "__ov";
      return candidate == familyPath ||
             candidate.rfind(templatedPrefix, 0) == 0 ||
             candidate.rfind(specializedPrefix, 0) == 0 ||
             candidate.rfind(overloadPrefix, 0) == 0;
    };
  [[maybe_unused]] auto &matchesResolvedFamilyPath = st.matchesResolvedFamilyPath;
  return PhaseStatus::Continue;
}

PhaseStatus SemanticsValidator::validateExprCallPhase7([[maybe_unused]] const std::vector<ParameterInfo> &params, [[maybe_unused]] const std::unordered_map<std::string, BindingInfo> &locals, [[maybe_unused]] const Expr &expr, ValidateExprCallState &st) {
  [[maybe_unused]] auto &enclosingStatements = st.enclosingStatements;
  [[maybe_unused]] auto &statementIndex = st.statementIndex;
  [[maybe_unused]] auto &expressionIsStatementContext = st.expressionIsStatementContext;
  [[maybe_unused]] auto publishExprRootDiagnostic = [&]() -> bool {
    captureExprContext(expr);
    return publishCurrentStructuredDiagnosticNow();
  };
  [[maybe_unused]] auto failExprRootDiagnostic = [&](std::string message) -> bool {
    return failExprDiagnostic(expr, std::move(message));
  };
  [[maybe_unused]] auto &dispatchBootstrap = st.dispatchBootstrap;
  [[maybe_unused]] auto &shouldBuiltinValidateBareKeyValueContainsCall = st.shouldBuiltinValidateBareKeyValueContainsCall;
  [[maybe_unused]] auto &shouldBuiltinValidateBareKeyValueAccessCall = st.shouldBuiltinValidateBareKeyValueAccessCall;
  [[maybe_unused]] auto &resolved = st.resolved;
  [[maybe_unused]] auto &resolvedMethod = st.resolvedMethod;
  [[maybe_unused]] auto &collectionDispatchSetup = st.collectionDispatchSetup;
  [[maybe_unused]] auto &matchesResolvedFamilyPath = st.matchesResolvedFamilyPath;
    std::string activeSpecializationSuffix;
    if (currentDefinitionContext_ != nullptr) {
      const size_t suffixPos = currentDefinitionContext_->fullPath.find("__t");
      if (suffixPos != std::string::npos) {
        activeSpecializationSuffix =
            currentDefinitionContext_->fullPath.substr(suffixPos);
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
  st.resolvedDefinition = it != defMap_.end() ? it->second : nullptr;
  [[maybe_unused]] auto &resolvedDefinition = st.resolvedDefinition;
    if (resolvedDefinition == nullptr && hasDefinitionFamilyPath(resolved)) {
      const Definition *firstMatch = nullptr;
      for (const auto &def : program_.definitions) {
        if (!matchesResolvedFamilyPath(def.fullPath, resolved)) {
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
  st.calleeParamsIt = paramsByDef_.find(resolved);
  [[maybe_unused]] auto &calleeParamsIt = st.calleeParamsIt;
    if (calleeParamsIt == paramsByDef_.end() && hasDefinitionFamilyPath(resolved)) {
      auto firstMatchIt = paramsByDef_.end();
      for (auto candidateIt = paramsByDef_.begin();
           candidateIt != paramsByDef_.end();
           ++candidateIt) {
        if (!matchesResolvedFamilyPath(candidateIt->first, resolved)) {
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
    if (!expr.isMethodCall && isResolvedKeyValueConstructorPath(resolved) &&
        expr.templateArgs.size() == 2) {
      std::string keyError;
      if (!validateBuiltinComparableKeyType(expr.templateArgs.front(), nullptr,
                                            keyError)) {
        return st.done(failExprRootDiagnostic(std::move(keyError)));
      }
      for (const Expr &arg : expr.args) {
        if (!validateExpr(params, locals, arg)) {
          return st.done(false);
        }
      }
      for (std::size_t i = 0; i < expr.args.size(); i += 2) {
        if (i + 1 >= expr.args.size()) {
          if (!validateExpr(params, locals, expr.args[i])) {
            return st.done(false);
          }
          break;
        }
        if (!this->validateCollectionElementType(
                expr.args[i], expr.templateArgs[0],
                "map constructor requires key type ", params, locals,
                dispatchBootstrap.dispatchResolvers)) {
          return st.done(false);
        }
        if (!this->validateCollectionElementType(
                expr.args[i + 1], expr.templateArgs[1],
                "map constructor requires value type ", params, locals,
                dispatchBootstrap.dispatchResolvers)) {
          return st.done(false);
        }
      }
    }
    ExprLateBuiltinContext lateBuiltinContext;
    prepareExprLateBuiltinContext(
        params,
        locals,
        dispatchBootstrap.dispatchResolverAdapters,
        dispatchBootstrap.dispatchResolvers,
        lateBuiltinContext);
    bool handledLateBuiltin = false;
    if (!validateExprLateBuiltins(params, locals, expr, resolved,
                                  resolvedMethod, lateBuiltinContext,
                                  handledLateBuiltin)) {
      return st.done(false);
    }
    if (handledLateBuiltin) {
      return st.done(true);
    }
    ExprCountCapacityBuiltinContext countCapacityBuiltinContext;
    prepareExprCountCapacityBuiltinContext(
        dispatchBootstrap.dispatchResolvers,
        countCapacityBuiltinContext);
    bool handledCountCapacityBuiltin = false;
    if (!validateExprCountCapacityBuiltins(
            params, locals, expr, resolved, resolvedMethod,
            countCapacityBuiltinContext, handledCountCapacityBuiltin)) {
      return st.done(false);
    }
    if (handledCountCapacityBuiltin) {
      return st.done(true);
    }
    const bool resolvedUsesCanonicalSoaNamespace =
        resolved.rfind(collection_paths::modulePrefix(
                           collection_paths::kLegacySoaVectorFolder),
                       0) == 0;
    std::string resolvedWithoutSpecialization = resolved;
    if (const size_t suffix = resolvedWithoutSpecialization.find("__");
        suffix != std::string::npos) {
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
        isLegacyOrCanonicalSoaHelperPath(resolvedSoaGetCanonical,
                                         collection_helpers::kGetRef) ||
        isCanonicalSoaRefLikeHelperPath(resolvedSoaRefCanonical) ||
        isExperimentalSoaGetLikeHelperPath(resolvedWithoutSpecialization) ||
        isExperimentalSoaRefLikeHelperPath(resolvedWithoutSpecialization);
    const bool resolvedIsSoaConversion =
        resolvedWithoutSpecialization == "/to_soa" ||
        isLegacyOrCanonicalSoaHelperPath(resolvedSoaToAosCanonical,
                                         "to_aos") ||
        isLegacyOrCanonicalSoaHelperPath(resolvedSoaToAosCanonical,
                                         collection_helpers::kToAosRef) ||
        isExperimentalSoaVectorConversionFamilyPath(
            resolvedWithoutSpecialization);
    const bool shouldLateValidateDirectSoaSurface =
        ((isSimpleCallName(expr, "get") ||
          isSimpleCallName(expr, collection_helpers::kGetRef) ||
          isSimpleCallName(expr, "ref") ||
          isSimpleCallName(expr, collection_helpers::kRefRef)) &&
         resolvedIsSoaAccess) ||
        ((isSimpleCallName(expr, "to_soa") ||
          isSimpleCallName(expr, "to_aos") ||
          isSimpleCallName(expr, collection_helpers::kToAosRef)) &&
         resolvedIsSoaConversion);
    const bool shouldLateValidateCanonicalSoaToAos =
        resolvedUsesCanonicalSoaNamespace &&
        isCanonicalStdlibSoaHelperPath(resolved, "to_aos");
    const bool shouldLateValidateCanonicalSoaToAosRef =
        resolvedUsesCanonicalSoaNamespace &&
        isCanonicalStdlibSoaHelperPath(resolved, collection_helpers::kToAosRef);
    if (resolvedDefinition == nullptr || resolvedMethod ||
        shouldLateValidateDirectSoaSurface || shouldLateValidateCanonicalSoaToAos ||
        shouldLateValidateCanonicalSoaToAosRef) {
      ExprLateMapSoaBuiltinContext lateMapSoaBuiltinContext;
      prepareExprLateMapSoaBuiltinContext(
          shouldBuiltinValidateBareKeyValueContainsCall,
          dispatchBootstrap.dispatchResolvers,
          lateMapSoaBuiltinContext);
      bool handledKeyValueSoaBuiltin = false;
      if (!validateExprLateMapSoaBuiltins(
              params, locals, expr, resolved, resolvedMethod,
              lateMapSoaBuiltinContext, handledKeyValueSoaBuiltin)) {
        return st.done(false);
      }
      if (handledKeyValueSoaBuiltin) {
        return st.done(true);
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
      if (!validateExprLateFallbackBuiltins(
              params, locals, expr, resolved, resolvedMethod,
              lateFallbackBuiltinContext, handledLateFallbackBuiltin)) {
        return st.done(false);
      }
      if (handledLateFallbackBuiltin) {
        return st.done(true);
      }
      bool handledMutationBorrowBuiltin = false;
      if (!validateExprMutationBorrowBuiltins(
              params, locals, expr, handledMutationBorrowBuiltin)) {
        return st.done(false);
      }
      if (handledMutationBorrowBuiltin) {
        return st.done(true);
      }
      ExprLateCallCompatibilityContext lateCallCompatibilityContext;
      prepareExprLateCallCompatibilityContext(
          dispatchBootstrap.dispatchResolvers,
          lateCallCompatibilityContext);
      bool handledLateCallCompatibility = false;
      if (!validateExprLateCallCompatibility(
              params, locals, expr, resolved,
              lateCallCompatibilityContext, handledLateCallCompatibility)) {
        return st.done(false);
      }
      if (handledLateCallCompatibility) {
        return st.done(true);
      }
      ExprLateUnknownTargetFallbackContext lateUnknownTargetFallbackContext;
      lateUnknownTargetFallbackContext.resolveMapTarget =
          dispatchBootstrap.resolveMapTarget;
      lateUnknownTargetFallbackContext.isIndexedArgsPackKeyValueReceiverTarget =
          [&](const Expr &target) {
            return this->isIndexedArgsPackKeyValueReceiverTarget(
                target, dispatchBootstrap.dispatchResolvers);
          };
      bool handledLateUnknownTargetFallback = false;
      if (!validateExprLateUnknownTargetFallbacks(
              params, locals, expr, lateUnknownTargetFallbackContext,
              handledLateUnknownTargetFallback)) {
        if (error_.empty()) {
          return st.done(failExprRootDiagnostic(
              "validateExprLateUnknownTargetFallbacks failed"));
        }
        return st.done(false);
      }
      if (handledLateUnknownTargetFallback) {
        return st.done(true);
      }
    }
  return PhaseStatus::Continue;
}

PhaseStatus SemanticsValidator::validateExprCallPhase8([[maybe_unused]] const std::vector<ParameterInfo> &params, [[maybe_unused]] const std::unordered_map<std::string, BindingInfo> &locals, [[maybe_unused]] const Expr &expr, ValidateExprCallState &st) {
  [[maybe_unused]] auto &enclosingStatements = st.enclosingStatements;
  [[maybe_unused]] auto &statementIndex = st.statementIndex;
  [[maybe_unused]] auto &expressionIsStatementContext = st.expressionIsStatementContext;
  [[maybe_unused]] auto publishExprRootDiagnostic = [&]() -> bool {
    captureExprContext(expr);
    return publishCurrentStructuredDiagnosticNow();
  };
  [[maybe_unused]] auto failExprRootDiagnostic = [&](std::string message) -> bool {
    return failExprDiagnostic(expr, std::move(message));
  };
  [[maybe_unused]] auto &dispatchBootstrap = st.dispatchBootstrap;
  [[maybe_unused]] auto &resolved = st.resolved;
  [[maybe_unused]] auto &hasMethodReceiverIndex = st.hasMethodReceiverIndex;
  [[maybe_unused]] auto &methodReceiverIndex = st.methodReceiverIndex;
  [[maybe_unused]] auto &resolvedDefinition = st.resolvedDefinition;
  [[maybe_unused]] auto &calleeParamsIt = st.calleeParamsIt;
    if (!expr.isMethodCall && isResolvedKeyValueConstructorPath(resolved) &&
        !(hasNamedArguments(expr.argNames) &&
          (defMap_.count(resolved) > 0 || hasImportedDefinitionPath(resolved)))) {
      if (hasNamedArguments(expr.argNames)) {
        return st.done(failExprRootDiagnostic(
            "named arguments not supported for builtin calls"));
      }
      if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
        return st.done(failExprRootDiagnostic(
            "block arguments require a definition target: " + resolved));
      }
      if (expr.templateArgs.size() == 2) {
        std::string keyError;
        if (!validateBuiltinComparableKeyType(expr.templateArgs.front(),
                                              nullptr, keyError)) {
          return st.done(failExprRootDiagnostic(std::move(keyError)));
        }
      }
      for (const Expr &arg : expr.args) {
        if (!validateExpr(params, locals, arg)) {
          return st.done(false);
        }
      }
      if (expr.templateArgs.size() == 2 && !expr.args.empty()) {
        const Definition *currentDef = nullptr;
        if (!currentValidationState_.context.definitionPath.empty()) {
          auto currentDefIt =
              defMap_.find(currentValidationState_.context.definitionPath);
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
                valueType, definitionNamespacePrefix, definitionTemplateArgs,
                visitingStructs)) {
          return st.done(failExprRootDiagnostic(
              std::string("map ") +
              "literal requires relocation-trivial map value type until container "
              "move/reallocation semantics are implemented: " +
              valueType));
        }
        for (std::size_t i = 0; i < expr.args.size(); i += 2) {
          if (i + 1 >= expr.args.size()) {
            if (!validateExpr(params, locals, expr.args[i])) {
              return st.done(false);
            }
            break;
          }
          if (!this->validateCollectionElementType(
                  expr.args[i], expr.templateArgs[0],
                  "map constructor requires key type ", params, locals,
                  dispatchBootstrap.dispatchResolvers)) {
            return st.done(false);
          }
          if (!this->validateCollectionElementType(
                  expr.args[i + 1], expr.templateArgs[1],
                  "map constructor requires value type ", params, locals,
                  dispatchBootstrap.dispatchResolvers)) {
            return st.done(false);
          }
        }
      }
      return st.done(true);
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
            return st.done(false);
          }
        }
        return st.done(true);
      }
      if (isStdNamespacedVectorCompatibilityHelperPath(resolved, "count") &&
          expr.args.size() != 1) {
        if (hasNamedArguments(expr.argNames)) {
          return st.done(failExprRootDiagnostic(
              "named arguments not supported for builtin calls"));
        }
        return st.done(failExprRootDiagnostic(
            "argument count mismatch for builtin count"));
      }
      if (isStdNamespacedVectorCompatibilityHelperPath(resolved, "capacity") &&
          expr.args.size() != 1) {
        if (hasNamedArguments(expr.argNames)) {
          return st.done(failExprRootDiagnostic(
              "named arguments not supported for builtin calls"));
        }
        return st.done(failExprRootDiagnostic(
            "argument count mismatch for builtin capacity"));
      }
      if (expr.isMethodCall &&
          isStdNamespacedVectorCompatibilityHelperPath(resolved, "count") &&
          expr.args.size() != 1) {
        if (hasNamedArguments(expr.argNames)) {
          return st.done(failExprRootDiagnostic(
              "named arguments not supported for builtin calls"));
        }
        return st.done(failExprRootDiagnostic(
            "argument count mismatch for builtin count"));
      }
      if (expr.isMethodCall &&
          isStdNamespacedVectorCompatibilityHelperPath(resolved, "capacity") &&
          expr.args.size() != 1) {
        if (hasNamedArguments(expr.argNames)) {
          return st.done(failExprRootDiagnostic(
              "named arguments not supported for builtin calls"));
        }
        return st.done(failExprRootDiagnostic(
            "argument count mismatch for builtin capacity"));
      }
      return st.done(failExprRootDiagnostic("unknown call target: " +
                                    unknownCallTarget));
    }
    const auto &calleeParams = calleeParamsIt->second;
    ExprResolvedCallSetup resolvedCallSetup;
    prepareExprResolvedCallSetup(
        params,
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
    if (!validateExprResolvedStructConstructorCall(
            params, locals, expr, resolved,
            resolvedCallSetup.resolvedStructConstructorContext,
            handledResolvedStructConstructor)) {
      return st.done(false);
    }
    if (handledResolvedStructConstructor) {
      return st.done(true);
    }
    bool handledResolvedCallArguments = false;
    if (!validateExprResolvedCallArguments(
            params, locals, expr, resolved,
            resolvedCallSetup.resolvedCallArgumentContext,
            handledResolvedCallArguments)) {
      if (error_.empty()) {
        return st.done(failExprRootDiagnostic(
            "validateExprResolvedCallArguments failed"));
      }
      return st.done(false);
    }
    if (handledResolvedCallArguments) {
      return st.done(true);
    }
  return PhaseStatus::Continue;
}

} // namespace primec::semantics
