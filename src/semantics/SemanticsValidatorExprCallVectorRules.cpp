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

// The rules that settle vector and field-view calls before any callee is looked up.

PhaseStatus SemanticsValidator::rewriteVectorAndFieldAccessCalls(ExprCallContext &cc) {
  const std::vector<ParameterInfo> &params = cc.params;
  const std::unordered_map<std::string, BindingInfo> &locals = cc.locals;
  const Expr &expr = cc.expr;
  [[maybe_unused]] auto publishExprRootDiagnostic = [&]() -> bool {
    captureExprContext(expr);
    return publishCurrentStructuredDiagnosticNow();
  };
  [[maybe_unused]] auto failExprRootDiagnostic = [&](std::string message) -> bool {
    return failExprDiagnostic(expr, std::move(message));
  };
  if (!expr.isMethodCall && isSimpleCallName(expr, "slice") && expr.args.size() == 3) {
    std::string receiverTypeText;
    const bool receiverIsArray =
        inferQueryExprTypeText(expr.args.front(), params, locals, receiverTypeText) && [&]() {
          const std::string typeText =
              normalizeBindingTypeName(unwrapReferencePointerTypeText(receiverTypeText));
          std::string base;
          std::string argText;
          return splitTemplateTypeName(typeText, base, argText) &&
                 normalizeBindingTypeName(base) == "array";
        }();
    if (receiverIsArray) {
      return cc.done(failExprRootDiagnostic(
          "slice(values, start, end) was removed; write values.slice(start, end) for a view"));
    }
  }
  if (expr.isFieldAccess) {
    return cc.done(validateExprFieldAccess(params, locals, expr));
  }
  bool handledFastNumericBuiltin = false;
  if (!expr.isMethodCall && !hasNamedArguments(expr.argNames) && expr.templateArgs.empty() &&
      !expr.hasBodyArguments && expr.bodyArguments.empty()) {
    std::string builtinName;
    std::string reflectedStructEqualityHelperPath;
    const bool shouldBypassFastNumericBuiltin =
        getBuiltinComparisonName(expr, builtinName) &&
        resolveReflectedStructEqualityHelperPath(
            params, locals, expr, builtinName, reflectedStructEqualityHelperPath);
    if (!shouldBypassFastNumericBuiltin) {
      if (!validateNumericBuiltinExpr(params, locals, expr, handledFastNumericBuiltin)) {
        return cc.done(false);
      }
      if (handledFastNumericBuiltin) {
        return cc.done(true);
      }
    }
  }
  cc.hasVectorHelperCallResolution = false;
  cc.vectorHelperCallReceiverIndex = 0;
  const std::string rootedVectorHelperPathPrefix(rootedVectorHelperPrefix());
  const std::string unrootedVectorHelperPathPrefix(unrootedVectorHelperPrefix());
  if (expr.isMethodCall && !expr.args.empty() &&
      (normalizeCollectionMethodName(expr.name) == "count" ||
       normalizeCollectionMethodName(expr.name) == "capacity" ||
       normalizeCollectionMethodName(expr.name) == "at" ||
       normalizeCollectionMethodName(expr.name) == "at_unsafe")) {
    std::string receiverTypeText;
    const bool receiverIsVector =
        inferQueryExprTypeText(expr.args.front(), params, locals, receiverTypeText) &&
        collection_helpers::isCollectionFamilyRoot(
            inferMethodCollectionTypePathFromTypeText(receiverTypeText),
            collection_helpers::CollectionFamily::Vector);
    if (receiverIsVector) {
      const std::string helperName = normalizeCollectionMethodName(expr.name);
      const bool explicitArrayNamespace =
          expr.namespacePrefix == "array" ||
          collection_helpers::isCollectionFamilyRoot(expr.namespacePrefix,
                                                     collection_helpers::CollectionFamily::Array) ||
          collection_helpers::isRootedArrayPath(expr.name);
      const bool explicitVectorNamespace =
          expr.namespacePrefix == "vector" ||
          collection_helpers::isCollectionFamilyRoot(
              expr.namespacePrefix, collection_helpers::CollectionFamily::Vector) ||
          expr.name.rfind(rootedVectorHelperPathPrefix, 0) == 0;
      if (explicitArrayNamespace) {
        return cc.done(failExprRootDiagnostic("unknown method: /array/" + helperName));
      }
      if (explicitVectorNamespace) {
        const std::string samePath = rootedVectorHelperPath(helperName);
        if (!hasDeclaredDefinitionPath(samePath) && !hasImportedDefinitionPath(samePath)) {
          return cc.done(failExprRootDiagnostic("unknown method: " + samePath));
        }
      }
      if ((helperName == "at" || helperName == "at_unsafe") && expr.args.size() == 2) {
        // A same-path user definition with a different index parameter
        // type (e.g. [bool] index instead of an integer) shadows this
        // integer-only check, but only when the call spells out the
        // explicit canonical path - same explicit-spelling-vs-sugar
        // distinction as the count/capacity same-path override in
        // SemanticsValidatorExprMethodResolution.cpp (see TODO-4721).
        const std::string canonicalVectorHelperPath =
            canonicalVectorCompatibilityHelperPathOrFallback(helperName);
        const auto canonicalHelperParamsIt = paramsByDef_.find(canonicalVectorHelperPath);
        const bool hasMatchingRealDefinitionArity =
            expr.name == canonicalVectorHelperPath &&
            canonicalHelperParamsIt != paramsByDef_.end() &&
            canonicalHelperParamsIt->second.size() == expr.args.size();
        if (!hasMatchingRealDefinitionArity) {
          const ReturnKind indexKind = inferExprReturnKind(expr.args[1], params, locals);
          if (indexKind != ReturnKind::Int && indexKind != ReturnKind::Int64 &&
              (indexKind != ReturnKind::UInt64 && indexKind != ReturnKind::UInt8)) {
            return cc.done(failExprRootDiagnostic(helperName + " requires integer index"));
          }
        }
      }
    }
  }
  if (expr.isMethodCall && !expr.args.empty()) {
    auto isVectorMutatorMethodName = [](std::string_view helperName) {
      return helperName == "push" || helperName == "pop" || helperName == "reserve" ||
             helperName == "clear" || helperName == "remove_at" || helperName == "remove_swap";
    };
    const std::string normalizedMutatorMethodName = normalizeCollectionMethodName(expr.name);
    if (isVectorMutatorMethodName(normalizedMutatorMethodName)) {
      std::string receiverTypeText;
      if (inferQueryExprTypeText(expr.args.front(), params, locals, receiverTypeText)) {
        const std::string receiverCollectionType =
            inferMethodCollectionTypePathFromTypeText(receiverTypeText);
        if (collection_helpers::isCollectionFamilyRoot(
                receiverCollectionType, collection_helpers::CollectionFamily::Array) ||
            collection_helpers::isCollectionFamilyRoot(
                receiverCollectionType, collection_helpers::CollectionFamily::String) ||
            collection_helpers::isCollectionFamilyRoot(receiverCollectionType,
                                                       collection_helpers::CollectionFamily::Map)) {
          return cc.done(
              failExprRootDiagnostic(normalizedMutatorMethodName + " requires vector binding"));
        }
      }
    }
    auto explicitVectorMutatorMethodPath = [&]() -> std::string {
      std::string normalizedPrefix = expr.namespacePrefix;
      if (!normalizedPrefix.empty() && normalizedPrefix.front() == '/') {
        normalizedPrefix.erase(normalizedPrefix.begin());
      }
      std::string normalizedName = expr.name;
      if (!normalizedName.empty() && normalizedName.front() == '/') {
        normalizedName.erase(normalizedName.begin());
      }
      if ((normalizedPrefix == "vector" ||
           isCanonicalVectorCompatibilityNamespace(normalizedPrefix)) &&
          isVectorMutatorMethodName(normalizedName)) {
        return "/" + normalizedPrefix + "/" + normalizedName;
      }
      if (normalizedName.rfind(unrootedVectorHelperPathPrefix, 0) == 0) {
        const std::string helperName = normalizedName.substr(unrootedVectorHelperPathPrefix.size());
        if (isVectorMutatorMethodName(helperName)) {
          return rootedVectorHelperPath(helperName);
        }
      }
      if (isUnrootedCanonicalVectorCompatibilityPath(normalizedName)) {
        const std::string helperName =
            std::string(stripUnrootedCanonicalVectorCompatibilityPrefix(normalizedName));
        if (isVectorMutatorMethodName(helperName)) {
          return "/" + normalizedName;
        }
      }
      return {};
    }();
    if (!explicitVectorMutatorMethodPath.empty()) {
      std::string receiverTypeText;
      if (const Expr &receiverExpr = expr.args.front(); receiverExpr.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(params, receiverExpr.name)) {
          receiverTypeText = bindingTypeText(*paramBinding);
        } else if (auto localIt = locals.find(receiverExpr.name); localIt != locals.end()) {
          receiverTypeText = bindingTypeText(localIt->second);
        }
      }
      const bool receiverIsVector =
          (!receiverTypeText.empty() ||
           inferQueryExprTypeText(expr.args.front(), params, locals, receiverTypeText)) &&
          ([&]() {
            if (collection_helpers::isCollectionFamilyRoot(
                    inferMethodCollectionTypePathFromTypeText(receiverTypeText),
                    collection_helpers::CollectionFamily::Vector)) {
              return true;
            }
            BindingInfo receiverBinding;
            std::string base;
            std::string argText;
            const std::string normalizedType = normalizeBindingTypeName(receiverTypeText);
            if (splitTemplateTypeName(normalizedType, base, argText)) {
              receiverBinding.typeName = normalizeBindingTypeName(base);
              receiverBinding.typeTemplateArg = argText;
            } else {
              receiverBinding.typeName = normalizedType;
            }
            std::string elemType;
            return extractCollectionVectorElementType(receiverBinding, elemType);
          })();
      const std::string rootedVectorShadowPath =
          rootedVectorHelperPath(normalizedMutatorMethodName);
      if (receiverIsVector && !hasDeclaredDefinitionPath(explicitVectorMutatorMethodPath) &&
          !hasImportedDefinitionPath(explicitVectorMutatorMethodPath) &&
          !hasDeclaredDefinitionPath(rootedVectorShadowPath) &&
          !hasImportedDefinitionPath(rootedVectorShadowPath)) {
        return cc.done(
            failExprRootDiagnostic("unknown method: " + explicitVectorMutatorMethodPath));
      }
    }
  }
  return PhaseStatus::Continue;
}

PhaseStatus SemanticsValidator::resolveVectorHelperCalls(ExprCallContext &cc) {
  const std::vector<ParameterInfo> &params = cc.params;
  const std::unordered_map<std::string, BindingInfo> &locals = cc.locals;
  const Expr &expr = cc.expr;
  [[maybe_unused]] auto &hasVectorHelperCallResolution = cc.hasVectorHelperCallResolution;
  [[maybe_unused]] auto &vectorHelperCallResolvedPath = cc.vectorHelperCallResolvedPath;
  [[maybe_unused]] auto &vectorHelperCallReceiverIndex = cc.vectorHelperCallReceiverIndex;
  [[maybe_unused]] auto &dispatchBootstrap = cc.dispatchBootstrap;
  [[maybe_unused]] auto &resolved = cc.resolved;
  [[maybe_unused]] auto &methodCompatibilitySetup = cc.methodCompatibilitySetup;
  [[maybe_unused]] auto &calleeParamsIt = cc.calleeParamsIt;
  [[maybe_unused]] auto &expressionIsStatementContext = cc.expressionIsStatementContext;
  [[maybe_unused]] auto publishExprRootDiagnostic = [&]() -> bool {
    captureExprContext(expr);
    return publishCurrentStructuredDiagnosticNow();
  };
  [[maybe_unused]] auto failExprRootDiagnostic = [&](std::string message) -> bool {
    return failExprDiagnostic(expr, std::move(message));
  };
  if (!resolveExprVectorHelperCall(params,
                                   locals,
                                   expr,
                                   expressionIsStatementContext,
                                   hasVectorHelperCallResolution,
                                   vectorHelperCallResolvedPath,
                                   vectorHelperCallReceiverIndex)) {
    return cc.done(false);
  }
  if (reportVectorMutatorPointerEscape(params, locals, expr)) {
    return cc.done(false);
  }
  std::string statementOnlyVectorMutatorName;
  if (expressionIsStatementContext &&
      getVectorMutatorHelperName(expr, statementOnlyVectorMutatorName) &&
      isPublishedVectorMutatorHelperName(statementOnlyVectorMutatorName)) {
    const std::string resolvedVectorMutatorTarget = vectorHelperCallResolvedPath.empty()
                                                        ? formatUnknownCallTarget(expr)
                                                        : vectorHelperCallResolvedPath;
    if (isStdNamespacedVectorCompatibilityHelperPath(resolvedVectorMutatorTarget,
                                                     statementOnlyVectorMutatorName)) {
      for (const Expr &arg : expr.args) {
        if (!validateExpr(params, locals, arg)) {
          return cc.done(false);
        }
      }
      return cc.done(true);
    }
  }
  if (isReturnCall(expr)) {
    return cc.done(failExprRootDiagnostic("return not allowed in expression context"));
  }
  auto isNonCtorWrapperReturnedBareVectorMethodWithoutHelper = [&]() {
    if (!expr.isMethodCall || expr.args.empty() ||
        (expr.name != "count" && expr.name != "capacity")) {
      return false;
    }
    const Expr &receiver = expr.args.front();
    if (receiver.kind != Expr::Kind::Call || receiver.isBinding || receiver.isMethodCall) {
      return false;
    }
    std::string receiverTypeText;
    if (!inferQueryExprTypeText(receiver, params, locals, receiverTypeText)) {
      return false;
    }
    std::string base;
    std::string argText;
    if (!splitTemplateTypeName(normalizeBindingTypeName(receiverTypeText), base, argText) ||
        normalizeBindingTypeName(base) != "vector") {
      return false;
    }
    const std::string resolvedReceiver = resolveCalleePath(receiver);
    if (isResolvedExperimentalVectorConstructorPath(resolvedReceiver)) {
      return false;
    }
    std::string builtinCollectionName;
    if (getBuiltinCollectionName(receiver, builtinCollectionName) &&
        builtinCollectionName == "vector") {
      return false;
    }
    const std::string methodPath = preferredBareVectorHelperTarget(expr.name);
    return !hasDeclaredDefinitionPath(methodPath) && !hasImportedDefinitionPath(methodPath);
  };
  if (isNonCtorWrapperReturnedBareVectorMethodWithoutHelper()) {
    return cc.done(failExprRootDiagnostic("unknown method: " + rootedVectorHelperPath(expr.name)));
  }
  if (std::string userStructAccessPath;
      resolveUserStructOwnAccessHelperCallPath(params, locals, expr, userStructAccessPath)) {
    // `values[k]` / `at(values, k)` on a user struct that declares its own
    // `at`: validate exactly like the explicit `/<Struct>/at(values, k)`
    // direct call, which is also the published direct-call target.
    Expr structAccessCall = expr;
    structAccessCall.name = std::move(userStructAccessPath);
    structAccessCall.namespacePrefix.clear();
    return cc.done(validateExpr(params, locals, structAccessCall));
  }
  prepareExprDispatchBootstrap(params, locals, dispatchBootstrap);
  if (!expr.isMethodCall && expr.namespacePrefix.empty() && expr.name == "at_unsafe" &&
      expr.args.size() >= 2 && dispatchBootstrap.dispatchResolvers.resolveMapTarget != nullptr) {
    std::string keyType;
    std::string valueType;
    const bool firstArgIsMap =
        dispatchBootstrap.dispatchResolvers.resolveMapTarget(expr.args.front(), keyType, valueType);
    bool laterArgIsMap = false;
    if (!firstArgIsMap) {
      for (size_t argIndex = 1; argIndex < expr.args.size(); ++argIndex) {
        if (dispatchBootstrap.dispatchResolvers.resolveMapTarget(
                expr.args[argIndex], keyType, valueType)) {
          laterArgIsMap = true;
          break;
        }
      }
    }
    if (!firstArgIsMap && laterArgIsMap) {
      return cc.done(
          failExprRootDiagnostic("argument type mismatch for /std/collections/map/at_unsafe"));
    }
  }
  cc.shouldBuiltinValidateBareKeyValueContainsCall =
      shouldBuiltinValidateCurrentMapWrapperHelper("contains");
  cc.shouldBuiltinValidateBareKeyValueAccessCall =
      shouldBuiltinValidateCurrentMapWrapperHelper("at") ||
      shouldBuiltinValidateCurrentMapWrapperHelper(collection_helpers::kAtRef) ||
      shouldBuiltinValidateCurrentMapWrapperHelper("at_unsafe") ||
      shouldBuiltinValidateCurrentMapWrapperHelper(collection_helpers::kAtUnsafeRef);
  bool handledEarlyPointerBuiltin = false;
  if (!validateExprEarlyPointerBuiltin(
          params, locals, expr, dispatchBootstrap, handledEarlyPointerBuiltin)) {
    return cc.done(false);
  }
  if (handledEarlyPointerBuiltin) {
    return cc.done(true);
  }
  if (const std::string removedRootMapDiagnostic = removedRootMapMethodDiagnostic(expr);
      !removedRootMapDiagnostic.empty()) {
    return cc.done(failExprRootDiagnostic(removedRootMapDiagnostic));
  }
  ExprPreDispatchDirectCallContext preDispatchDirectCallContext;
  preDispatchDirectCallContext.dispatchBootstrap = &dispatchBootstrap;
  std::optional<Expr> rewrittenPreDispatchDirectCall;
  bool handledPreDispatchDirectCall = false;
  if (!validateExprPreDispatchDirectCalls(params,
                                          locals,
                                          expr,
                                          preDispatchDirectCallContext,
                                          resolved,
                                          rewrittenPreDispatchDirectCall,
                                          handledPreDispatchDirectCall)) {
    return cc.done(false);
  }
  if (rewrittenPreDispatchDirectCall.has_value()) {
    return cc.done(validateExpr(params, locals, *rewrittenPreDispatchDirectCall));
  }
  if (!expr.isMethodCall && expr.args.size() == 2 && !expr.name.empty() &&
      expr.name.front() == '/' &&
      expr.name.rfind(primec::collection_paths::modulePrefix(primec::collection_paths::kMapFolder),
                      0) == 0) {
    auto calleeParamsIt = paramsByDef_.find(expr.name);
    if (calleeParamsIt != paramsByDef_.end() && !calleeParamsIt->second.empty()) {
      const BindingInfo &entriesBinding = calleeParamsIt->second.front().binding;
      std::string expectedKeyType;
      std::string expectedValueType;
      std::string actualKeyType;
      std::string actualValueType;
      if (extractExperimentalKeyValueFieldTypesFromStructPath(
              normalizeBindingTypeName(entriesBinding.typeName),
              expectedKeyType,
              expectedValueType,
              /*includeCanonicalMapValue=*/true) &&
          dispatchBootstrap.dispatchResolvers.resolveMapTarget != nullptr &&
          dispatchBootstrap.dispatchResolvers.resolveMapTarget(
              expr.args.front(), actualKeyType, actualValueType) &&
          (normalizeBindingTypeName(expectedKeyType) != normalizeBindingTypeName(actualKeyType) ||
           normalizeBindingTypeName(expectedValueType) !=
               normalizeBindingTypeName(actualValueType))) {
        return cc.done(failExprRootDiagnostic("argument type mismatch for " + expr.name +
                                              " parameter " + calleeParamsIt->second.front().name));
      }
    }
  }
  if (handledPreDispatchDirectCall) {
    return cc.done(true);
  }
  if (!prepareExprMethodCompatibilitySetup(params,
                                           locals,
                                           expr,
                                           dispatchBootstrap,
                                           hasVectorHelperCallResolution,
                                           vectorHelperCallResolvedPath,
                                           vectorHelperCallReceiverIndex,
                                           resolved,
                                           methodCompatibilitySetup)) {
    return cc.done(false);
  }
  cc.resolvedMethod = methodCompatibilitySetup.resolvedMethod;
  cc.usedMethodTarget = methodCompatibilitySetup.usedMethodTarget;
  return PhaseStatus::Continue;
}

} // namespace primec::semantics
