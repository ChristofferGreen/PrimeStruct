#include "SemanticsValidator.h"
#include "primec/StringLiteral.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <functional>
#include <iomanip>
#include <optional>
#include <sstream>
#include <unordered_set>

namespace primec::semantics {

bool SemanticsValidator::validateExpr(const std::vector<ParameterInfo> &params,
                                      const std::unordered_map<std::string, BindingInfo> &locals,
                                      const Expr &expr,
                                      const std::vector<Expr> *enclosingStatements,
                                      size_t statementIndex) {
  ExprContextScope exprScope(*this, expr);
  if (expr.isLambda) {
    return validateLambdaExpr(params, locals, expr, enclosingStatements, statementIndex);
  }
  if (!allowEntryArgStringUse_) {
    if (isEntryArgsAccess(expr) || isEntryArgStringBinding(locals, expr)) {
      error_ = "entry argument strings are only supported in print calls or string bindings";
      return false;
    }
  }
  std::optional<EffectScope> effectScope;
  if (expr.kind == Expr::Kind::Call && !expr.isBinding && !expr.transforms.empty()) {
    std::unordered_set<std::string> executionEffects;
    if (!resolveExecutionEffects(expr, executionEffects)) {
      return false;
    }
    effectScope.emplace(*this, std::move(executionEffects));
  }
  if (expr.kind == Expr::Kind::Literal) {
    return true;
  }
  if (expr.kind == Expr::Kind::BoolLiteral) {
    return true;
  }
  if (expr.kind == Expr::Kind::FloatLiteral) {
    return true;
  }
  if (expr.kind == Expr::Kind::StringLiteral) {
    ParsedStringLiteral parsed;
    if (!parseStringLiteralToken(expr.stringValue, parsed, error_)) {
      return false;
    }
    if (parsed.encoding == StringEncoding::Ascii && !isAsciiText(parsed.decoded)) {
      error_ = "ascii string literal contains non-ASCII characters";
      return false;
    }
    return true;
  }
  if (expr.kind == Expr::Kind::Name) {
    if (isParam(params, expr.name) || locals.count(expr.name) > 0) {
      if (currentValidationContext_.movedBindings.count(expr.name) > 0) {
        error_ = "use-after-move: " + expr.name;
        return false;
      }
      return true;
    }
    if (!allowMathBareName(expr.name) && expr.name.find('/') == std::string::npos &&
        isBuiltinMathConstant(expr.name, true)) {
      error_ = "math constant requires import /std/math/* or /std/math/<name>: " + expr.name;
      return false;
    }
    if (isBuiltinMathConstant(expr.name, allowMathBareName(expr.name))) {
      return true;
    }
    error_ = "unknown identifier: " + expr.name;
    return false;
  }
  if (expr.kind == Expr::Kind::Call) {
    if (expr.isBinding) {
      error_ = "binding not allowed in expression context";
      return false;
    }
    std::optional<EffectScope> effectScope;
    if (!expr.transforms.empty()) {
      std::unordered_set<std::string> executionEffects;
      if (!resolveExecutionEffects(expr, executionEffects)) {
        return false;
      }
      effectScope.emplace(*this, std::move(executionEffects));
    }
    if (isMatchCall(expr)) {
      Expr expanded;
      if (!lowerMatchToIf(expr, expanded, error_)) {
        return false;
      }
      return validateExpr(params, locals, expanded);
    }
    if (isIfCall(expr)) {
      return validateIfExpr(params, locals, expr);
    }
    if (!expr.isMethodCall && isSimpleCallName(expr, "uninitialized")) {
      if (hasNamedArguments(expr.argNames)) {
        error_ = "named arguments not supported for builtin calls";
        return false;
      }
      if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
        error_ = "uninitialized does not accept block arguments";
        return false;
      }
      if (!expr.args.empty()) {
        error_ = "uninitialized does not accept arguments";
        return false;
      }
      if (expr.templateArgs.size() != 1) {
        error_ = "uninitialized requires exactly one template argument";
        return false;
      }
      return true;
    }
    if (!expr.isMethodCall && (isSimpleCallName(expr, "init") ||
                               isSimpleCallName(expr, "drop") ||
                               isSimpleCallName(expr, "take") ||
                               isSimpleCallName(expr, "borrow"))) {
      const std::string name = expr.name;
      auto isUninitializedStorage = [&](const Expr &arg) -> bool {
        BindingInfo binding;
        bool resolved = false;
        if (!resolveUninitializedStorageBinding(params, locals, arg, binding, resolved)) {
          return false;
        }
        if (!resolved || binding.typeName != "uninitialized" || binding.typeTemplateArg.empty()) {
          return false;
        }
        return true;
      };
      const bool treatAsUninitializedHelper =
          (name != "take") || (!expr.args.empty() && isUninitializedStorage(expr.args.front()));
      if (treatAsUninitializedHelper) {
        if (hasNamedArguments(expr.argNames)) {
          error_ = "named arguments not supported for builtin calls";
          return false;
        }
        if (!expr.templateArgs.empty()) {
          error_ = name + " does not accept template arguments";
          return false;
        }
        if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
          error_ = name + " does not accept block arguments";
          return false;
        }
        const size_t expectedArgs = (name == "init") ? 2 : 1;
        if (expr.args.size() != expectedArgs) {
          error_ = name + " requires exactly " + std::to_string(expectedArgs) + " argument" +
                   (expectedArgs == 1 ? "" : "s");
          return false;
        }
        if (name == "init" || name == "drop") {
          error_ = name + " is only supported as a statement";
          return false;
        }
        for (const auto &arg : expr.args) {
          if (!validateExpr(params, locals, arg)) {
            return false;
          }
        }
        if (!isUninitializedStorage(expr.args.front())) {
          error_ = name + " requires uninitialized<T> storage";
          return false;
        }
        return true;
      }
    }
    if (isBuiltinBlockCall(expr) && expr.hasBodyArguments) {
      return validateBlockExpr(params, locals, expr);
    }
    if (isBuiltinBlockCall(expr)) {
      error_ = "block requires block arguments";
      return false;
    }
    if (isLoopCall(expr)) {
      error_ = "loop is only supported as a statement";
      return false;
    }
    if (isWhileCall(expr)) {
      error_ = "while is only supported as a statement";
      return false;
    }
    if (isForCall(expr)) {
      error_ = "for is only supported as a statement";
      return false;
    }
    if (isRepeatCall(expr)) {
      error_ = "repeat is only supported as a statement";
      return false;
    }
    if (isSimpleCallName(expr, "dispatch")) {
      error_ = "dispatch is only supported as a statement";
      return false;
    }
    if (isSimpleCallName(expr, "buffer_store")) {
      error_ = "buffer_store is only supported as a statement";
      return false;
    }
    bool hasVectorHelperCallResolution = false;
    std::string vectorHelperCallResolvedPath;
    size_t vectorHelperCallReceiverIndex = 0;
    if (!resolveExprVectorHelperCall(params,
                                     locals,
                                     expr,
                                     hasVectorHelperCallResolution,
                                     vectorHelperCallResolvedPath,
                                     vectorHelperCallReceiverIndex)) {
      return false;
    }
    if (isReturnCall(expr)) {
      error_ = "return not allowed in expression context";
      return false;
    }
    ExprDispatchBootstrap dispatchBootstrap;
    prepareExprDispatchBootstrap(params, locals, dispatchBootstrap);
    const bool shouldBuiltinValidateBareMapCountCall =
        shouldBuiltinValidateCurrentMapWrapperHelper("count");
    const bool shouldBuiltinValidateBareMapContainsCall =
        shouldBuiltinValidateCurrentMapWrapperHelper("contains");
    const bool shouldBuiltinValidateBareMapTryAtCall =
        shouldBuiltinValidateCurrentMapWrapperHelper("tryAt");
    const bool shouldBuiltinValidateBareMapAccessCall =
        shouldBuiltinValidateCurrentMapWrapperHelper("at") ||
        shouldBuiltinValidateCurrentMapWrapperHelper("at_unsafe");
    bool handledEarlyPointerBuiltin = false;
    if (!validateExprEarlyPointerBuiltin(
            params, locals, expr, dispatchBootstrap,
            handledEarlyPointerBuiltin)) {
      return false;
    }
    if (handledEarlyPointerBuiltin) {
      return true;
    }

    std::string resolved;
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
      return false;
    }
    if (rewrittenPreDispatchDirectCall.has_value()) {
      return validateExpr(params, locals, *rewrittenPreDispatchDirectCall);
    }
    if (handledPreDispatchDirectCall) {
      return true;
    }
    ExprMethodCompatibilitySetup methodCompatibilitySetup;
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
      return false;
    }
    bool resolvedMethod = methodCompatibilitySetup.resolvedMethod;
    bool usedMethodTarget = methodCompatibilitySetup.usedMethodTarget;
    bool hasMethodReceiverIndex = methodCompatibilitySetup.hasMethodReceiverIndex;
    size_t methodReceiverIndex = methodCompatibilitySetup.methodReceiverIndex;
    if (expr.isFieldAccess) {
      return validateExprFieldAccess(params, locals, expr);
    }
    ExprCollectionDispatchSetup collectionDispatchSetup;
    if (!prepareExprCollectionDispatchSetup(
            params,
            locals,
            expr,
            dispatchBootstrap.dispatchResolvers,
            dispatchBootstrap.dispatchResolverAdapters,
            resolved,
            collectionDispatchSetup)) {
      return false;
    }
    ExprMethodResolutionContext methodResolutionContext;
    methodResolutionContext.hasVectorHelperCallResolution =
        hasVectorHelperCallResolution;
    methodResolutionContext.promoteCapacityToBuiltinValidation =
        methodCompatibilitySetup.promoteCapacityToBuiltinValidation;
    methodResolutionContext.unavailableMethodDiagnostic =
        methodCompatibilitySetup.unavailableMethodDiagnostic;
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
        error_ = "validateExprMethodCallTarget failed name=" + expr.name +
                 " ns=" + expr.namespacePrefix +
                 " resolved=" + resolved +
                 " templateArgs=" + std::to_string(expr.templateArgs.size()) +
                 " args=" + std::to_string(expr.args.size()) +
                 " receiver=" + receiverSummary;
      }
      return false;
    }
    resolved = resolveExprConcreteCallPath(params, locals, expr, resolved);
    ExprCollectionCountCapacityDispatchContext collectionCountCapacityDispatchContext;
    collectionCountCapacityDispatchContext.isNamespacedVectorHelperCall =
        collectionDispatchSetup.isNamespacedVectorHelperCall;
    collectionCountCapacityDispatchContext.namespacedHelper =
        collectionDispatchSetup.namespacedHelper;
    collectionCountCapacityDispatchContext.isStdNamespacedVectorCountCall =
        collectionDispatchSetup.isStdNamespacedVectorCountCall;
    collectionCountCapacityDispatchContext
        .shouldBuiltinValidateStdNamespacedVectorCountCall =
        collectionDispatchSetup.shouldBuiltinValidateStdNamespacedVectorCountCall;
    collectionCountCapacityDispatchContext.isStdNamespacedMapCountCall =
        collectionDispatchSetup.isStdNamespacedMapCountCall;
    collectionCountCapacityDispatchContext.isNamespacedVectorCountCall =
        collectionDispatchSetup.isNamespacedVectorCountCall;
    collectionCountCapacityDispatchContext.isNamespacedMapCountCall =
        collectionDispatchSetup.isNamespacedMapCountCall;
    collectionCountCapacityDispatchContext
        .isUnnamespacedMapCountFallbackCall =
        collectionDispatchSetup.isUnnamespacedMapCountFallbackCall;
    collectionCountCapacityDispatchContext.isResolvedMapCountCall =
        collectionDispatchSetup.isResolvedMapCountCall;
    collectionCountCapacityDispatchContext
        .prefersCanonicalVectorCountAliasDefinition =
        collectionDispatchSetup.prefersCanonicalVectorCountAliasDefinition;
    collectionCountCapacityDispatchContext
        .isStdNamespacedVectorCapacityCall =
        collectionDispatchSetup.isStdNamespacedVectorCapacityCall;
    collectionCountCapacityDispatchContext
        .shouldBuiltinValidateStdNamespacedVectorCapacityCall =
        collectionDispatchSetup
            .shouldBuiltinValidateStdNamespacedVectorCapacityCall;
    collectionCountCapacityDispatchContext.isNamespacedVectorCapacityCall =
        collectionDispatchSetup.isNamespacedVectorCapacityCall;
    collectionCountCapacityDispatchContext
        .isDirectStdNamespacedVectorCountWrapperMapTarget =
        collectionDispatchSetup.isDirectStdNamespacedVectorCountWrapperMapTarget;
    collectionCountCapacityDispatchContext
        .hasStdNamespacedVectorCountAliasDefinition =
        collectionDispatchSetup.hasStdNamespacedVectorCountAliasDefinition;
    collectionCountCapacityDispatchContext.shouldBuiltinValidateBareMapCountCall =
        shouldBuiltinValidateBareMapCountCall;
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
      return false;
    }
    if (handledCollectionCountCapacityTarget &&
        rewrittenCollectionCountCapacityCall.has_value()) {
      return validateExpr(params, locals,
                          *rewrittenCollectionCountCapacityCall);
    }
    ExprDirectCollectionFallbackContext directCollectionFallbackContext;
    directCollectionFallbackContext.isStdNamespacedVectorCountCall =
        collectionDispatchSetup.isStdNamespacedVectorCountCall;
    directCollectionFallbackContext.dispatchResolvers =
        &dispatchBootstrap.dispatchResolvers;
    std::optional<Expr> rewrittenDirectCollectionFallbackCall;
    if (!validateExprDirectCollectionFallbacks(
            params,
            locals,
            expr,
            resolved,
            directCollectionFallbackContext,
            rewrittenDirectCollectionFallbackCall)) {
      return false;
    }
    if (rewrittenDirectCollectionFallbackCall.has_value()) {
      return validateExpr(params, locals,
                          *rewrittenDirectCollectionFallbackCall);
    }
    ExprCollectionAccessDispatchContext collectionAccessDispatchContext;
    prepareExprCollectionAccessDispatchContext(
        collectionDispatchSetup,
        shouldBuiltinValidateBareMapContainsCall,
        shouldBuiltinValidateBareMapAccessCall,
        dispatchBootstrap.dispatchResolvers,
        dispatchBootstrap.resolveMapTarget,
        collectionAccessDispatchContext);
    bool handledCollectionAccessTarget = false;
    if (!resolveExprCollectionAccessTarget(
            params, locals, expr, collectionAccessDispatchContext,
            handledCollectionAccessTarget, resolved, resolvedMethod,
            usedMethodTarget, hasMethodReceiverIndex,
            methodReceiverIndex)) {
      return false;
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
      return false;
    }
    if (handledPostAccessPrecheck) {
      return true;
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
        error_ = "validateExprNamedArguments failed";
      }
      return false;
    }
    auto it = defMap_.find(resolved);
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
      return false;
    }
    if (handledLateBuiltin) {
      return true;
    }
    ExprCountCapacityMapBuiltinContext countCapacityMapBuiltinContext;
    prepareExprCountCapacityMapBuiltinContext(
        collectionDispatchSetup.shouldBuiltinValidateStdNamespacedVectorCountCall,
        collectionDispatchSetup.isStdNamespacedVectorCountCall,
        shouldBuiltinValidateBareMapCountCall,
        collectionDispatchSetup.isNamespacedMapCountCall,
        collectionDispatchSetup.isResolvedMapCountCall,
        collectionDispatchSetup
            .shouldBuiltinValidateStdNamespacedVectorCapacityCall,
        collectionDispatchSetup.isStdNamespacedVectorCapacityCall,
        dispatchBootstrap.dispatchResolverAdapters,
        dispatchBootstrap.dispatchResolvers,
        countCapacityMapBuiltinContext);
    bool handledCountCapacityMapBuiltin = false;
    if (!validateExprCountCapacityMapBuiltins(
            params, locals, expr, resolved, resolvedMethod,
            countCapacityMapBuiltinContext, handledCountCapacityMapBuiltin)) {
      return false;
    }
    if (handledCountCapacityMapBuiltin) {
      return true;
    }
    if (it == defMap_.end() || resolvedMethod) {
      ExprLateMapSoaBuiltinContext lateMapSoaBuiltinContext;
      prepareExprLateMapSoaBuiltinContext(
          shouldBuiltinValidateBareMapContainsCall,
          dispatchBootstrap.dispatchResolvers,
          lateMapSoaBuiltinContext);
      bool handledMapSoaBuiltin = false;
      if (!validateExprLateMapSoaBuiltins(
              params, locals, expr, resolved, resolvedMethod,
              lateMapSoaBuiltinContext, handledMapSoaBuiltin)) {
        return false;
      }
      if (handledMapSoaBuiltin) {
        return true;
      }
      ExprLateFallbackBuiltinContext lateFallbackBuiltinContext;
      prepareExprLateFallbackBuiltinContext(
          collectionDispatchSetup.isStdNamespacedVectorAccessCall,
          collectionDispatchSetup.shouldAllowStdAccessCompatibilityFallback,
          collectionDispatchSetup.hasStdNamespacedVectorAccessDefinition,
          collectionDispatchSetup.isStdNamespacedMapAccessCall,
          collectionDispatchSetup.hasStdNamespacedMapAccessDefinition,
          shouldBuiltinValidateBareMapAccessCall,
          dispatchBootstrap.dispatchResolvers,
          lateFallbackBuiltinContext);
      bool handledLateFallbackBuiltin = false;
      if (!validateExprLateFallbackBuiltins(
              params, locals, expr, resolved, resolvedMethod,
              lateFallbackBuiltinContext, handledLateFallbackBuiltin)) {
        return false;
      }
      if (handledLateFallbackBuiltin) {
        return true;
      }
      bool handledMutationBorrowBuiltin = false;
      if (!validateExprMutationBorrowBuiltins(
              params, locals, expr, handledMutationBorrowBuiltin)) {
        return false;
      }
      if (handledMutationBorrowBuiltin) {
        return true;
      }
      ExprLateCallCompatibilityContext lateCallCompatibilityContext;
      prepareExprLateCallCompatibilityContext(
          dispatchBootstrap.dispatchResolvers,
          lateCallCompatibilityContext);
      bool handledLateCallCompatibility = false;
      if (!validateExprLateCallCompatibility(
              params, locals, expr, resolved,
              lateCallCompatibilityContext, handledLateCallCompatibility)) {
        return false;
      }
      if (handledLateCallCompatibility) {
        return true;
      }
      ExprLateMapAccessBuiltinContext lateMapAccessBuiltinContext;
      prepareExprLateMapAccessBuiltinContext(
          dispatchBootstrap.dispatchResolvers,
          shouldBuiltinValidateBareMapContainsCall,
          shouldBuiltinValidateBareMapTryAtCall,
          shouldBuiltinValidateBareMapAccessCall,
          lateMapAccessBuiltinContext);
      bool handledLateMapAccessBuiltin = false;
      if (!validateExprLateMapAccessBuiltins(
              params, locals, expr, resolved, lateMapAccessBuiltinContext,
              handledLateMapAccessBuiltin)) {
        return false;
      }
      if (handledLateMapAccessBuiltin) {
        return true;
      }
      ExprLateUnknownTargetFallbackContext lateUnknownTargetFallbackContext;
      lateUnknownTargetFallbackContext.resolveMapTarget =
          dispatchBootstrap.resolveMapTarget;
      bool handledLateUnknownTargetFallback = false;
      if (!validateExprLateUnknownTargetFallbacks(
              params, locals, expr, lateUnknownTargetFallbackContext,
              handledLateUnknownTargetFallback)) {
        if (error_.empty()) {
          error_ = "validateExprLateUnknownTargetFallbacks failed";
        }
        return false;
      }
      if (handledLateUnknownTargetFallback) {
        return true;
      }
    }
    const auto &calleeParams = paramsByDef_[resolved];
    ExprResolvedCallSetup resolvedCallSetup;
    prepareExprResolvedCallSetup(
        params,
        locals,
        expr,
        resolved,
        dispatchBootstrap.dispatchResolvers,
        *it->second,
        calleeParams,
        hasMethodReceiverIndex,
        methodReceiverIndex,
        resolvedCallSetup);
    bool handledResolvedStructConstructor = false;
    if (!validateExprResolvedStructConstructorCall(
            params, locals, expr, resolved,
            resolvedCallSetup.resolvedStructConstructorContext,
            handledResolvedStructConstructor)) {
      return false;
    }
    if (handledResolvedStructConstructor) {
      return true;
    }
    bool handledResolvedCallArguments = false;
    if (!validateExprResolvedCallArguments(
            params, locals, expr, resolved,
            resolvedCallSetup.resolvedCallArgumentContext,
            handledResolvedCallArguments)) {
      if (error_.empty()) {
        error_ = "validateExprResolvedCallArguments failed";
      }
      return false;
    }
    if (handledResolvedCallArguments) {
      return true;
    }
  }
  return false;
}

} // namespace primec::semantics
