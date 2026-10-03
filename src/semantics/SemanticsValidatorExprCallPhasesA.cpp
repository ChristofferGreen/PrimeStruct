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

PhaseStatus SemanticsValidator::validateExprCallPhase3([[maybe_unused]] const std::vector<ParameterInfo> &params, [[maybe_unused]] const std::unordered_map<std::string, BindingInfo> &locals, [[maybe_unused]] const Expr &expr, ValidateExprCallState &st) {
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
    if (!expr.isMethodCall && isSimpleCallName(expr, "slice") &&
        expr.args.size() == 3) {
      auto isIntegerRangeKind = [](ReturnKind kind) {
        return kind == ReturnKind::Int || kind == ReturnKind::Int64 ||
               kind == ReturnKind::UInt64;
      };
      auto literalIntegerValue = [](const Expr &candidate) -> std::optional<int64_t> {
        if (candidate.kind != Expr::Kind::Literal || candidate.isUnsigned) {
          return std::nullopt;
        }
        if (candidate.intWidth == 64) {
          return static_cast<int64_t>(candidate.literalValue);
        }
        return static_cast<int32_t>(candidate.literalValue);
      };
      auto literalArrayExtent = [&](const Expr &candidate) -> std::optional<int64_t> {
        std::string collectionName;
        if (candidate.kind != Expr::Kind::Call ||
            !getBuiltinCollectionName(candidate, collectionName) ||
            collectionName != "array") {
          return std::nullopt;
        }
        return static_cast<int64_t>(candidate.args.size());
      };
      std::string receiverTypeText;
      const bool receiverIsArray =
          inferQueryExprTypeText(expr.args.front(), params, locals,
                                 receiverTypeText) &&
          [&]() {
            const std::string typeText = normalizeBindingTypeName(
                unwrapReferencePointerTypeText(receiverTypeText));
            std::string base;
            std::string argText;
            return splitTemplateTypeName(typeText, base, argText) &&
                   normalizeBindingTypeName(base) == "array";
          }();
      if (receiverIsArray) {
        if (hasNamedArguments(expr.argNames)) {
          return st.done(failExprRootDiagnostic("named arguments not supported for builtin calls"));
        }
        if (!expr.templateArgs.empty()) {
          return st.done(failExprRootDiagnostic("slice does not accept template arguments"));
        }
        if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
          return st.done(failExprRootDiagnostic("slice does not accept block arguments"));
        }
        const ReturnKind startKind =
            inferExprReturnKind(expr.args[1], params, locals);
        const ReturnKind endKind = inferExprReturnKind(expr.args[2], params, locals);
        if (!isIntegerRangeKind(startKind) || !isIntegerRangeKind(endKind)) {
          return st.done(failExprRootDiagnostic("slice requires integer start and end"));
        }
        if (const auto extent = literalArrayExtent(expr.args.front())) {
          const auto start = literalIntegerValue(expr.args[1]);
          const auto end = literalIntegerValue(expr.args[2]);
          if (start.has_value() && end.has_value() &&
              (*start < 0 || *end < *start || *end > *extent)) {
            return st.done(failExprRootDiagnostic(
                "slice range out of bounds: start=" + std::to_string(*start) +
                ", end=" + std::to_string(*end) +
                ", count=" + std::to_string(*extent)));
          }
        }
        return st.done(validateExpr(params, locals, expr.args.front(),
                            enclosingStatements, statementIndex) &&
               validateExpr(params, locals, expr.args[1],
                            enclosingStatements, statementIndex) &&
               validateExpr(params, locals, expr.args[2],
                            enclosingStatements, statementIndex));
      }
    }
    if (expr.isFieldAccess) {
      return st.done(validateExprFieldAccess(params, locals, expr));
    }
    bool handledFastNumericBuiltin = false;
    if (!expr.isMethodCall &&
        !hasNamedArguments(expr.argNames) &&
        expr.templateArgs.empty() &&
        !expr.hasBodyArguments &&
        expr.bodyArguments.empty()) {
      std::string builtinName;
      std::string reflectedStructEqualityHelperPath;
      const bool shouldBypassFastNumericBuiltin =
          getBuiltinComparisonName(expr, builtinName) &&
          resolveReflectedStructEqualityHelperPath(
              params,
              locals,
              expr,
              builtinName,
              reflectedStructEqualityHelperPath);
      if (!shouldBypassFastNumericBuiltin) {
        if (!validateNumericBuiltinExpr(params, locals, expr,
                                        handledFastNumericBuiltin)) {
          return st.done(false);
        }
        if (handledFastNumericBuiltin) {
          return st.done(true);
        }
      }
    }
  st.hasVectorHelperCallResolution = false;
  [[maybe_unused]] auto &hasVectorHelperCallResolution = st.hasVectorHelperCallResolution;
  [[maybe_unused]] auto &vectorHelperCallResolvedPath = st.vectorHelperCallResolvedPath;
  st.vectorHelperCallReceiverIndex = 0;
  [[maybe_unused]] auto &vectorHelperCallReceiverIndex = st.vectorHelperCallReceiverIndex;
    const std::string rootedVectorHelperPathPrefix(rootedVectorHelperPrefix());
    const std::string unrootedVectorHelperPathPrefix(unrootedVectorHelperPrefix());
    if (expr.isMethodCall && !expr.args.empty() &&
        (normalizeCollectionMethodName(expr.name) == "count" ||
         normalizeCollectionMethodName(expr.name) == "capacity" ||
         normalizeCollectionMethodName(expr.name) == "at" ||
         normalizeCollectionMethodName(expr.name) == "at_unsafe")) {
      std::string receiverTypeText;
      const bool receiverIsVector =
          inferQueryExprTypeText(expr.args.front(), params, locals,
                                 receiverTypeText) &&
          collection_helpers::isCollectionFamilyRoot(inferMethodCollectionTypePathFromTypeText(receiverTypeText), collection_helpers::CollectionFamily::Vector);
      if (receiverIsVector) {
        const std::string helperName = normalizeCollectionMethodName(expr.name);
        const bool explicitArrayNamespace =
            expr.namespacePrefix == "array" ||
            collection_helpers::isCollectionFamilyRoot(expr.namespacePrefix, collection_helpers::CollectionFamily::Array) ||
            collection_helpers::isRootedArrayPath(expr.name);
        const bool explicitVectorNamespace =
            expr.namespacePrefix == "vector" ||
            collection_helpers::isCollectionFamilyRoot(expr.namespacePrefix, collection_helpers::CollectionFamily::Vector) ||
            expr.name.rfind(rootedVectorHelperPathPrefix, 0) == 0;
        if (explicitArrayNamespace) {
          return st.done(failExprRootDiagnostic("unknown method: /array/" + helperName));
        }
        if (explicitVectorNamespace) {
          const std::string samePath = rootedVectorHelperPath(helperName);
          if (!hasDeclaredDefinitionPath(samePath) &&
              !hasImportedDefinitionPath(samePath)) {
            return st.done(failExprRootDiagnostic("unknown method: " + samePath));
          }
        }
        if ((helperName == "at" || helperName == "at_unsafe") &&
            expr.args.size() == 2) {
          // A same-path user definition with a different index parameter
          // type (e.g. [bool] index instead of an integer) shadows this
          // integer-only check, but only when the call spells out the
          // explicit canonical path - same explicit-spelling-vs-sugar
          // distinction as the count/capacity same-path override in
          // SemanticsValidatorExprMethodResolution.cpp (see TODO-4721).
          const std::string canonicalVectorHelperPath =
              canonicalVectorCompatibilityHelperPathOrFallback(helperName);
          const auto canonicalHelperParamsIt =
              paramsByDef_.find(canonicalVectorHelperPath);
          const bool hasMatchingRealDefinitionArity =
              expr.name == canonicalVectorHelperPath &&
              canonicalHelperParamsIt != paramsByDef_.end() &&
              canonicalHelperParamsIt->second.size() == expr.args.size();
          if (!hasMatchingRealDefinitionArity) {
            const ReturnKind indexKind =
                inferExprReturnKind(expr.args[1], params, locals);
            if (indexKind != ReturnKind::Int &&
                indexKind != ReturnKind::Int64 &&
                indexKind != ReturnKind::UInt64) {
              return st.done(failExprRootDiagnostic(helperName +
                                            " requires integer index"));
            }
          }
        }
      }
    }
    if (expr.isMethodCall && !expr.args.empty()) {
      auto isVectorMutatorMethodName = [](std::string_view helperName) {
        return helperName == "push" || helperName == "pop" ||
               helperName == "reserve" || helperName == "clear" ||
               helperName == "remove_at" || helperName == "remove_swap";
      };
      const std::string normalizedMutatorMethodName =
          normalizeCollectionMethodName(expr.name);
      if (isVectorMutatorMethodName(normalizedMutatorMethodName)) {
        std::string receiverTypeText;
        if (inferQueryExprTypeText(expr.args.front(), params, locals,
                                   receiverTypeText)) {
          const std::string receiverCollectionType =
              inferMethodCollectionTypePathFromTypeText(receiverTypeText);
          if (collection_helpers::isCollectionFamilyRoot(receiverCollectionType, collection_helpers::CollectionFamily::Array) ||
              collection_helpers::isCollectionFamilyRoot(receiverCollectionType, collection_helpers::CollectionFamily::String) ||
              collection_helpers::isCollectionFamilyRoot(receiverCollectionType, collection_helpers::CollectionFamily::Map)) {
            return st.done(failExprRootDiagnostic(
                normalizedMutatorMethodName + " requires vector binding"));
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
          const std::string helperName =
              normalizedName.substr(unrootedVectorHelperPathPrefix.size());
          if (isVectorMutatorMethodName(helperName)) {
            return rootedVectorHelperPath(helperName);
          }
        }
        if (isUnrootedCanonicalVectorCompatibilityPath(normalizedName)) {
          const std::string helperName =
              std::string(stripUnrootedCanonicalVectorCompatibilityPrefix(
                  normalizedName));
          if (isVectorMutatorMethodName(helperName)) {
            return "/" + normalizedName;
          }
        }
        return {};
      }();
      if (!explicitVectorMutatorMethodPath.empty()) {
        std::string receiverTypeText;
        if (const Expr &receiverExpr = expr.args.front();
            receiverExpr.kind == Expr::Kind::Name) {
          if (const BindingInfo *paramBinding =
                  findParamBinding(params, receiverExpr.name)) {
            receiverTypeText = bindingTypeText(*paramBinding);
          } else if (auto localIt = locals.find(receiverExpr.name);
                     localIt != locals.end()) {
            receiverTypeText = bindingTypeText(localIt->second);
          }
        }
        const bool receiverIsVector =
            (!receiverTypeText.empty() ||
             inferQueryExprTypeText(expr.args.front(), params, locals,
                                    receiverTypeText)) &&
            ([&]() {
              if (collection_helpers::isCollectionFamilyRoot(inferMethodCollectionTypePathFromTypeText(receiverTypeText), collection_helpers::CollectionFamily::Vector)) {
                return true;
              }
              BindingInfo receiverBinding;
              std::string base;
              std::string argText;
              const std::string normalizedType =
                  normalizeBindingTypeName(receiverTypeText);
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
        if (receiverIsVector &&
            !hasDeclaredDefinitionPath(explicitVectorMutatorMethodPath) &&
            !hasImportedDefinitionPath(explicitVectorMutatorMethodPath) &&
            !hasDeclaredDefinitionPath(rootedVectorShadowPath) &&
            !hasImportedDefinitionPath(rootedVectorShadowPath)) {
          return st.done(failExprRootDiagnostic("unknown method: " +
                                        explicitVectorMutatorMethodPath));
        }
      }
    }
  return PhaseStatus::Continue;
}

PhaseStatus SemanticsValidator::validateExprCallPhase4([[maybe_unused]] const std::vector<ParameterInfo> &params, [[maybe_unused]] const std::unordered_map<std::string, BindingInfo> &locals, [[maybe_unused]] const Expr &expr, ValidateExprCallState &st) {
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
  [[maybe_unused]] auto &vectorHelperCallResolvedPath = st.vectorHelperCallResolvedPath;
  [[maybe_unused]] auto &vectorHelperCallReceiverIndex = st.vectorHelperCallReceiverIndex;
    if (!resolveExprVectorHelperCall(params,
                                     locals,
                                     expr,
                                     expressionIsStatementContext,
                                     hasVectorHelperCallResolution,
                                     vectorHelperCallResolvedPath,
                                     vectorHelperCallReceiverIndex)) {
      return st.done(false);
    }
    std::string statementOnlyVectorMutatorName;
    if (expressionIsStatementContext &&
        getVectorMutatorHelperName(expr, statementOnlyVectorMutatorName) &&
        isPublishedVectorMutatorHelperName(statementOnlyVectorMutatorName)) {
      const std::string resolvedVectorMutatorTarget =
          vectorHelperCallResolvedPath.empty() ? formatUnknownCallTarget(expr)
                                               : vectorHelperCallResolvedPath;
      if (isStdNamespacedVectorCompatibilityHelperPath(
              resolvedVectorMutatorTarget, statementOnlyVectorMutatorName)) {
        for (const Expr &arg : expr.args) {
          if (!validateExpr(params, locals, arg)) {
            return st.done(false);
          }
        }
        return st.done(true);
      }
    }
    if (isReturnCall(expr)) {
      return st.done(failExprRootDiagnostic("return not allowed in expression context"));
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
      return !hasDeclaredDefinitionPath(methodPath) &&
             !hasImportedDefinitionPath(methodPath);
    };
    if (isNonCtorWrapperReturnedBareVectorMethodWithoutHelper()) {
      return st.done(failExprRootDiagnostic("unknown method: " +
                                    rootedVectorHelperPath(expr.name)));
    }
    if (std::string userStructAccessPath;
        resolveUserStructOwnAccessHelperCallPath(
            params, locals, expr, userStructAccessPath)) {
      // `values[k]` / `at(values, k)` on a user struct that declares its own
      // `at`: validate exactly like the explicit `/<Struct>/at(values, k)`
      // direct call, which is also the published direct-call target.
      Expr structAccessCall = expr;
      structAccessCall.name = std::move(userStructAccessPath);
      structAccessCall.namespacePrefix.clear();
      return st.done(validateExpr(params, locals, structAccessCall));
    }
  [[maybe_unused]] auto &dispatchBootstrap = st.dispatchBootstrap;
    prepareExprDispatchBootstrap(params, locals, dispatchBootstrap);
    if (!expr.isMethodCall && expr.namespacePrefix.empty() &&
        expr.name == "at_unsafe" && expr.args.size() >= 2 &&
        dispatchBootstrap.dispatchResolvers.resolveMapTarget != nullptr) {
      std::string keyType;
      std::string valueType;
      const bool firstArgIsMap =
          dispatchBootstrap.dispatchResolvers.resolveMapTarget(
              expr.args.front(), keyType, valueType);
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
        return st.done(failExprRootDiagnostic(
            "argument type mismatch for /std/collections/map/at_unsafe"));
      }
    }
  st.shouldBuiltinValidateBareKeyValueContainsCall = shouldBuiltinValidateCurrentMapWrapperHelper("contains");
  [[maybe_unused]] auto &shouldBuiltinValidateBareKeyValueContainsCall = st.shouldBuiltinValidateBareKeyValueContainsCall;
  st.shouldBuiltinValidateBareKeyValueAccessCall = shouldBuiltinValidateCurrentMapWrapperHelper("at") ||
        shouldBuiltinValidateCurrentMapWrapperHelper(collection_helpers::kAtRef) ||
        shouldBuiltinValidateCurrentMapWrapperHelper("at_unsafe") ||
        shouldBuiltinValidateCurrentMapWrapperHelper(collection_helpers::kAtUnsafeRef);
  [[maybe_unused]] auto &shouldBuiltinValidateBareKeyValueAccessCall = st.shouldBuiltinValidateBareKeyValueAccessCall;
    bool handledEarlyPointerBuiltin = false;
    if (!validateExprEarlyPointerBuiltin(
            params, locals, expr, dispatchBootstrap,
            handledEarlyPointerBuiltin)) {
      return st.done(false);
    }
    if (handledEarlyPointerBuiltin) {
      return st.done(true);
    }
  [[maybe_unused]] auto &resolved = st.resolved;
    if (const std::string removedRootMapDiagnostic =
            removedRootMapMethodDiagnostic(expr);
        !removedRootMapDiagnostic.empty()) {
      return st.done(failExprRootDiagnostic(removedRootMapDiagnostic));
    }
    ExprPreDispatchDirectCallContext preDispatchDirectCallContext;
    preDispatchDirectCallContext.dispatchBootstrap = &dispatchBootstrap;
    std::optional<Expr> rewrittenPreDispatchDirectCall;
    bool handledPreDispatchDirectCall = false;
    if (!validateExprPreDispatchDirectCalls(
            params,
            locals,
            expr,
            preDispatchDirectCallContext,
            resolved,
            rewrittenPreDispatchDirectCall,
            handledPreDispatchDirectCall)) {
      return st.done(false);
    }
    if (rewrittenPreDispatchDirectCall.has_value()) {
      return st.done(validateExpr(params, locals, *rewrittenPreDispatchDirectCall));
    }
    if (!expr.isMethodCall && expr.args.size() == 2 && !expr.name.empty() &&
        expr.name.front() == '/' &&
        expr.name.rfind(primec::collection_paths::modulePrefix(
                            primec::collection_paths::kMapFolder),
                        0) == 0) {
      auto calleeParamsIt = paramsByDef_.find(expr.name);
      if (calleeParamsIt != paramsByDef_.end() && !calleeParamsIt->second.empty()) {
        const BindingInfo &entriesBinding = calleeParamsIt->second.front().binding;
        std::string expectedKeyType;
        std::string expectedValueType;
        std::string actualKeyType;
        std::string actualValueType;
        if (extractExperimentalKeyValueFieldTypesFromStructPath(
                normalizeBindingTypeName(entriesBinding.typeName), expectedKeyType,
                expectedValueType, /*includeCanonicalMapValue=*/true) &&
            dispatchBootstrap.dispatchResolvers.resolveMapTarget != nullptr &&
            dispatchBootstrap.dispatchResolvers.resolveMapTarget(
                expr.args.front(), actualKeyType, actualValueType) &&
            (normalizeBindingTypeName(expectedKeyType) !=
                 normalizeBindingTypeName(actualKeyType) ||
             normalizeBindingTypeName(expectedValueType) !=
                 normalizeBindingTypeName(actualValueType))) {
          return st.done(failExprRootDiagnostic("argument type mismatch for " + expr.name +
                                        " parameter " +
                                        calleeParamsIt->second.front().name));
        }
      }
    }
    if (handledPreDispatchDirectCall) {
      return st.done(true);
    }
  [[maybe_unused]] auto &methodCompatibilitySetup = st.methodCompatibilitySetup;
    if (!prepareExprMethodCompatibilitySetup(
            params,
            locals,
            expr,
            dispatchBootstrap,
            hasVectorHelperCallResolution,
            vectorHelperCallResolvedPath,
            vectorHelperCallReceiverIndex,
            resolved,
            methodCompatibilitySetup)) {
      return st.done(false);
    }
  st.resolvedMethod = methodCompatibilitySetup.resolvedMethod;
  [[maybe_unused]] auto &resolvedMethod = st.resolvedMethod;
  st.usedMethodTarget = methodCompatibilitySetup.usedMethodTarget;
  [[maybe_unused]] auto &usedMethodTarget = st.usedMethodTarget;
  return PhaseStatus::Continue;
}

PhaseStatus SemanticsValidator::validateExprCallPhase5([[maybe_unused]] const std::vector<ParameterInfo> &params, [[maybe_unused]] const std::unordered_map<std::string, BindingInfo> &locals, [[maybe_unused]] const Expr &expr, ValidateExprCallState &st) {
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
  [[maybe_unused]] auto &resolved = st.resolved;
  [[maybe_unused]] auto &methodCompatibilitySetup = st.methodCompatibilitySetup;
  [[maybe_unused]] auto &resolvedMethod = st.resolvedMethod;
  [[maybe_unused]] auto &usedMethodTarget = st.usedMethodTarget;
  st.hasMethodReceiverIndex = methodCompatibilitySetup.hasMethodReceiverIndex;
  [[maybe_unused]] auto &hasMethodReceiverIndex = st.hasMethodReceiverIndex;
  st.methodReceiverIndex = methodCompatibilitySetup.methodReceiverIndex;
  [[maybe_unused]] auto &methodReceiverIndex = st.methodReceiverIndex;
  [[maybe_unused]] auto &collectionDispatchSetup = st.collectionDispatchSetup;
    if (!prepareExprCollectionDispatchSetup(
            params,
            locals,
            expr,
            dispatchBootstrap.dispatchResolvers,
            dispatchBootstrap.dispatchResolverAdapters,
            resolved,
            collectionDispatchSetup)) {
      return st.done(false);
    }
    ExprMethodResolutionContext methodResolutionContext;
    methodResolutionContext.hasVectorHelperCallResolution =
        hasVectorHelperCallResolution;
    methodResolutionContext.promoteCapacityToBuiltinValidation =
        methodCompatibilitySetup.promoteCapacityToBuiltinValidation;
    methodResolutionContext.unavailableMethodDiagnostic =
        methodCompatibilitySetup.unavailableMethodDiagnostic;
    const bool isIndexedArgsPackKeyValueMethodReceiver = [&]() {
      if (!expr.isMethodCall || expr.args.empty() ||
          !this->isIndexedArgsPackKeyValueReceiverTarget(
              expr.args.front(), dispatchBootstrap.dispatchResolvers)) {
        return false;
      }
      std::string methodName = normalizeCollectionMethodName(expr.name);
      const size_t slash = methodName.find_last_of('/');
      if (slash != std::string::npos) {
        methodName = methodName.substr(slash + 1);
      }
      return collection_helpers::isCountHelperName(methodName) ||
             methodName == "size" ||
             collection_helpers::isContainsHelperName(methodName) ||
             collection_helpers::isTryAtHelperName(methodName) ||
             collection_helpers::isAtHelperName(methodName) ||
             collection_helpers::isAtUnsafeHelperName(methodName) ||
             collection_helpers::isInsertHelperName(methodName);
    }();
    if (expr.isMethodCall && !expr.args.empty() &&
        !isIndexedArgsPackKeyValueMethodReceiver &&
        expr.args.front().kind == Expr::Kind::Call) {
      if (!validateExpr(params, locals, expr.args.front())) {
        return st.done(false);
      }
    }
    if (!validateExprMethodCallTarget(
            params,
            locals,
            expr,
            methodResolutionContext,
            dispatchBootstrap.dispatchResolvers,
            dispatchBootstrap.dispatchResolverAdapters,
            resolved,
            resolvedMethod,
            usedMethodTarget,
            hasMethodReceiverIndex,
            methodReceiverIndex)) {
      if (error_.empty()) {
        auto exprKindName = [](Expr::Kind kind) -> const char * {
          switch (kind) {
          case Expr::Kind::Literal:
            return "Literal";
          case Expr::Kind::BoolLiteral:
            return "BoolLiteral";
          case Expr::Kind::FloatLiteral:
            return "FloatLiteral";
          case Expr::Kind::StringLiteral:
            return "StringLiteral";
          case Expr::Kind::Call:
            return "Call";
          case Expr::Kind::Name:
            return "Name";
          }
          return "Unknown";
        };
        std::string receiverSummary = "none";
        if (!expr.args.empty()) {
          const Expr &receiver = expr.args.front();
          receiverSummary = std::string(exprKindName(receiver.kind)) +
                            ":" + receiver.name +
                            " ns=" + receiver.namespacePrefix;
        }
        return st.done(failExprRootDiagnostic(
            "validateExprMethodCallTarget failed name=" + expr.name +
            " ns=" + expr.namespacePrefix +
            " resolved=" + resolved +
            " templateArgs=" + std::to_string(expr.templateArgs.size()) +
            " args=" + std::to_string(expr.args.size()) +
            " receiver=" + receiverSummary));
      }
      return st.done(false);
    }
    resolved = resolveExprConcreteCallPath(params, locals, expr, resolved);
    auto stripSpecializationSuffix = [](const std::string &path) -> std::string {
      const size_t suffix = path.find("__t");
      return suffix == std::string::npos ? path : path.substr(0, suffix);
    };
    auto isDestroyHelperPath = [&](const std::string &path) -> bool {
      const std::string base = stripSpecializationSuffix(path);
      static const std::array<std::string_view, 4> suffixes = {
          "/Destroy", "/DestroyStack", "/DestroyHeap", "/DestroyBuffer"};
      for (std::string_view suffix : suffixes) {
        if (base.size() >= suffix.size() &&
            base.compare(base.size() - suffix.size(), suffix.size(),
                         suffix.data(), suffix.size()) == 0) {
          return true;
        }
      }
      return false;
    };
    auto isSoaGrowthHelperPath = [&](const std::string &path) -> bool {
      const std::string base = stripSpecializationSuffix(path);
      if (isLegacyOrCanonicalSoaHelperPath(base, "push") ||
          isLegacyOrCanonicalSoaHelperPath(base, "reserve")) {
        return true;
      }
      return isExperimentalSoaGrowthHelperPath(base);
    };
    if (isDestroyHelperPath(resolved) || isSoaGrowthHelperPath(resolved)) {
      auto resolveNamedBinding =
          [&](const std::string &name) -> const BindingInfo * {
        if (const BindingInfo *paramBinding = findParamBinding(params, name)) {
          return paramBinding;
        }
        auto it = locals.find(name);
        if (it != locals.end()) {
          return &it->second;
        }
        return nullptr;
      };
      auto isSoaBorrowBinding = [&](const BindingInfo &binding) -> bool {
        if (binding.typeName == "soa") {
          return true;
        }
        std::string elemType;
        if (extractExperimentalSoaVectorElementType(binding, elemType)) {
          return true;
        }
        const std::string normalizedType =
            normalizeBindingTypeName(binding.typeName);
        if ((normalizedType == "Reference" || normalizedType == "Pointer") &&
            !binding.typeTemplateArg.empty()) {
          std::string base;
          std::string arg;
          const std::string normalizedTarget =
              normalizeBindingTypeName(binding.typeTemplateArg);
          if (splitTemplateTypeName(normalizedTarget, base, arg)) {
            return normalizeBindingTypeName(base) == "soa";
          }
          return normalizedTarget == "soa";
        }
        return false;
      };
      auto isSoaFieldViewBindingType = [&](const BindingInfo &binding) -> bool {
        return isSoaFieldViewTypePath(binding.typeName);
      };
      auto referenceRootForBorrowBinding =
          [&](const std::string &bindingName,
              const BindingInfo &binding) -> std::string {
        if (binding.typeName != "Reference" &&
            !isSoaFieldViewBindingType(binding) &&
            !(binding.typeName == "auto" && !binding.referenceRoot.empty())) {
          return "";
        }
        if (!binding.referenceRoot.empty()) {
          return binding.referenceRoot;
        }
        return bindingName;
      };
      auto hasActiveBorrowForRoot =
          [&](const std::string &borrowRoot,
              const std::string &ignoreBorrowName = std::string()) -> bool {
        if (borrowRoot.empty() ||
            currentValidationState_.context.definitionIsUnsafe) {
          return false;
        }
        auto hasBorrowFrom =
            [&](const std::string &bindingName,
                const BindingInfo &binding) -> bool {
          if (!ignoreBorrowName.empty() &&
              bindingName == ignoreBorrowName) {
            return false;
          }
          if (currentValidationState_.endedReferenceBorrows.count(bindingName) >
              0) {
            return false;
          }
          const std::string root =
              referenceRootForBorrowBinding(bindingName, binding);
          return !root.empty() && root == borrowRoot;
        };
        for (const auto &param : params) {
          if (hasBorrowFrom(param.name, param.binding)) {
            return true;
          }
        }
        for (const auto &entry : locals) {
          if (hasBorrowFrom(entry.first, entry.second)) {
            return true;
          }
        }
        return false;
      };
      auto resolveStandaloneSoaBorrowRoot =
          [&](const Expr &receiverExpr,
              std::string &borrowRootOut,
              std::string &ignoreBorrowNameOut) -> bool {
        borrowRootOut.clear();
        ignoreBorrowNameOut.clear();
        if (receiverExpr.kind == Expr::Kind::Name) {
          const BindingInfo *binding =
              resolveNamedBinding(receiverExpr.name);
          if (binding == nullptr || !isSoaBorrowBinding(*binding)) {
            return false;
          }
          const std::string normalizedType =
              normalizeBindingTypeName(binding->typeName);
          if (normalizedType == "Reference" || normalizedType == "Pointer") {
            ignoreBorrowNameOut = receiverExpr.name;
            if (!binding->referenceRoot.empty()) {
              borrowRootOut = binding->referenceRoot;
            } else {
              borrowRootOut = receiverExpr.name;
            }
            return true;
          }
          borrowRootOut = receiverExpr.name;
          return true;
        }
        if (receiverExpr.kind != Expr::Kind::Call) {
          return false;
        }
        std::string builtinName;
        if (getBuiltinPointerName(receiverExpr, builtinName) &&
            builtinName == "dereference" &&
            receiverExpr.args.size() == 1) {
          const Expr &pointerExpr = receiverExpr.args.front();
          if (pointerExpr.kind == Expr::Kind::Name) {
            const BindingInfo *binding =
                resolveNamedBinding(pointerExpr.name);
            if (binding == nullptr || binding->referenceRoot.empty() ||
                !isSoaBorrowBinding(*binding)) {
              return false;
            }
            ignoreBorrowNameOut = pointerExpr.name;
            borrowRootOut = binding->referenceRoot;
            return true;
          }
          if (getBuiltinPointerName(pointerExpr, builtinName) &&
              builtinName == "location" && pointerExpr.args.size() == 1 &&
              pointerExpr.args.front().kind == Expr::Kind::Name) {
            const BindingInfo *binding =
                resolveNamedBinding(pointerExpr.args.front().name);
            if (binding == nullptr || !isSoaBorrowBinding(*binding)) {
              return false;
            }
            borrowRootOut = pointerExpr.args.front().name;
            return true;
          }
        }
        return false;
      };
      const Expr *receiverExpr = nullptr;
      if (hasMethodReceiverIndex && methodReceiverIndex < expr.args.size()) {
        receiverExpr = &expr.args[methodReceiverIndex];
      } else if (expr.isMethodCall && !expr.args.empty()) {
        receiverExpr = &expr.args.front();
      } else if (!expr.args.empty()) {
        receiverExpr = &expr.args.front();
      }
      if (receiverExpr != nullptr) {
        std::string borrowRoot;
        std::string ignoreBorrowName;
        if (resolveStandaloneSoaBorrowRoot(*receiverExpr, borrowRoot,
                                           ignoreBorrowName) &&
            hasActiveBorrowForRoot(borrowRoot, ignoreBorrowName)) {
          const std::string borrowSink =
              !ignoreBorrowName.empty() ? ignoreBorrowName : borrowRoot;
          return st.done(failExprRootDiagnostic(
              "borrowed binding: " + borrowRoot + " (root: " + borrowRoot +
              ", sink: " + borrowSink + ")"));
        }
      }
    }
  return PhaseStatus::Continue;
}

} // namespace primec::semantics
