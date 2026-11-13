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

// The rules that resolve method and collection targets of a call.

PhaseStatus SemanticsValidator::resolveCollectionMethodTargets(ExprCallContext &cc) {
  const std::vector<ParameterInfo> &params = cc.params;
  const std::unordered_map<std::string, BindingInfo> &locals = cc.locals;
  const Expr &expr = cc.expr;
  [[maybe_unused]] auto &hasVectorHelperCallResolution = cc.hasVectorHelperCallResolution;
  [[maybe_unused]] auto &dispatchBootstrap = cc.dispatchBootstrap;
  [[maybe_unused]] auto &resolved = cc.resolved;
  [[maybe_unused]] auto &methodCompatibilitySetup = cc.methodCompatibilitySetup;
  [[maybe_unused]] auto &resolvedMethod = cc.resolvedMethod;
  [[maybe_unused]] auto &usedMethodTarget = cc.usedMethodTarget;
  [[maybe_unused]] auto &hasMethodReceiverIndex = cc.hasMethodReceiverIndex;
  [[maybe_unused]] auto &methodReceiverIndex = cc.methodReceiverIndex;
  [[maybe_unused]] auto &collectionDispatchSetup = cc.collectionDispatchSetup;
  [[maybe_unused]] auto publishExprRootDiagnostic = [&]() -> bool {
    captureExprContext(expr);
    return publishCurrentStructuredDiagnosticNow();
  };
  [[maybe_unused]] auto failExprRootDiagnostic = [&](std::string message) -> bool {
    return failExprDiagnostic(expr, std::move(message));
  };
  cc.hasMethodReceiverIndex = methodCompatibilitySetup.hasMethodReceiverIndex;
  cc.methodReceiverIndex = methodCompatibilitySetup.methodReceiverIndex;
  if (!prepareExprCollectionDispatchSetup(params,
                                          locals,
                                          expr,
                                          dispatchBootstrap.dispatchResolvers,
                                          dispatchBootstrap.dispatchResolverAdapters,
                                          resolved,
                                          collectionDispatchSetup)) {
    return cc.done(false);
  }
  ExprMethodResolutionContext methodResolutionContext;
  methodResolutionContext.hasVectorHelperCallResolution = hasVectorHelperCallResolution;
  methodResolutionContext.promoteCapacityToBuiltinValidation =
      methodCompatibilitySetup.promoteCapacityToBuiltinValidation;
  methodResolutionContext.unavailableMethodDiagnostic =
      methodCompatibilitySetup.unavailableMethodDiagnostic;
  const bool isIndexedArgsPackKeyValueMethodReceiver = [&]() {
    if (!expr.isMethodCall || expr.args.empty() ||
        !this->isIndexedArgsPackKeyValueReceiverTarget(expr.args.front(),
                                                       dispatchBootstrap.dispatchResolvers)) {
      return false;
    }
    std::string methodName = normalizeCollectionMethodName(expr.name);
    const size_t slash = methodName.find_last_of('/');
    if (slash != std::string::npos) {
      methodName = methodName.substr(slash + 1);
    }
    return collection_helpers::isCountHelperName(methodName) || methodName == "size" ||
           collection_helpers::isContainsHelperName(methodName) ||
           collection_helpers::isTryAtHelperName(methodName) ||
           collection_helpers::isAtHelperName(methodName) ||
           collection_helpers::isAtUnsafeHelperName(methodName) ||
           collection_helpers::isInsertHelperName(methodName);
  }();
  if (expr.isMethodCall && !expr.args.empty() && !isIndexedArgsPackKeyValueMethodReceiver &&
      expr.args.front().kind == Expr::Kind::Call) {
    if (!validateExpr(params, locals, expr.args.front())) {
      return cc.done(false);
    }
  }
  if (!validateExprMethodCallTarget(params,
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
        receiverSummary = std::string(exprKindName(receiver.kind)) + ":" + receiver.name +
                          " ns=" + receiver.namespacePrefix;
      }
      return cc.done(failExprRootDiagnostic(
          "validateExprMethodCallTarget failed name=" + expr.name + " ns=" + expr.namespacePrefix +
          " resolved=" + resolved + " templateArgs=" + std::to_string(expr.templateArgs.size()) +
          " args=" + std::to_string(expr.args.size()) + " receiver=" + receiverSummary));
    }
    return cc.done(false);
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
          base.compare(base.size() - suffix.size(), suffix.size(), suffix.data(), suffix.size()) ==
              0) {
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
    auto resolveNamedBinding = [&](const std::string &name) -> const BindingInfo * {
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
      const std::string normalizedType = normalizeBindingTypeName(binding.typeName);
      if ((normalizedType == "Reference" || normalizedType == "Pointer") &&
          !binding.typeTemplateArg.empty()) {
        std::string base;
        std::string arg;
        const std::string normalizedTarget = normalizeBindingTypeName(binding.typeTemplateArg);
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
    auto referenceRootForBorrowBinding = [&](const std::string &bindingName,
                                             const BindingInfo &binding) -> std::string {
      if (binding.typeName != "Reference" && !isSoaFieldViewBindingType(binding) &&
          !(binding.typeName == "auto" && !binding.referenceRoot.empty()) &&
          !binding.isContainerView) {
        return "";
      }
      if (!binding.referenceRoot.empty()) {
        return binding.referenceRoot;
      }
      return bindingName;
    };
    auto hasActiveBorrowForRoot = [&](const std::string &borrowRoot,
                                      const std::string &ignoreBorrowName = std::string()) -> bool {
      if (borrowRoot.empty() || currentValidationState_.context.definitionIsUnsafe) {
        return false;
      }
      auto hasBorrowFrom = [&](const std::string &bindingName, const BindingInfo &binding) -> bool {
        if (!ignoreBorrowName.empty() && bindingName == ignoreBorrowName) {
          return false;
        }
        if (currentValidationState_.endedReferenceBorrows.count(bindingName) > 0) {
          return false;
        }
        const std::string root = referenceRootForBorrowBinding(bindingName, binding);
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
    auto resolveStandaloneSoaBorrowRoot = [&](const Expr &receiverExpr,
                                              std::string &borrowRootOut,
                                              std::string &ignoreBorrowNameOut) -> bool {
      borrowRootOut.clear();
      ignoreBorrowNameOut.clear();
      if (receiverExpr.kind == Expr::Kind::Name) {
        const BindingInfo *binding = resolveNamedBinding(receiverExpr.name);
        if (binding == nullptr || !isSoaBorrowBinding(*binding)) {
          return false;
        }
        const std::string normalizedType = normalizeBindingTypeName(binding->typeName);
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
      if (getBuiltinPointerName(receiverExpr, builtinName) && builtinName == "dereference" &&
          receiverExpr.args.size() == 1) {
        const Expr &pointerExpr = receiverExpr.args.front();
        if (pointerExpr.kind == Expr::Kind::Name) {
          const BindingInfo *binding = resolveNamedBinding(pointerExpr.name);
          if (binding == nullptr || binding->referenceRoot.empty() ||
              !isSoaBorrowBinding(*binding)) {
            return false;
          }
          ignoreBorrowNameOut = pointerExpr.name;
          borrowRootOut = binding->referenceRoot;
          return true;
        }
        if (getBuiltinPointerName(pointerExpr, builtinName) && builtinName == "location" &&
            pointerExpr.args.size() == 1 && pointerExpr.args.front().kind == Expr::Kind::Name) {
          const BindingInfo *binding = resolveNamedBinding(pointerExpr.args.front().name);
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
      if (resolveStandaloneSoaBorrowRoot(*receiverExpr, borrowRoot, ignoreBorrowName) &&
          hasActiveBorrowForRoot(borrowRoot, ignoreBorrowName)) {
        const std::string borrowSink = !ignoreBorrowName.empty() ? ignoreBorrowName : borrowRoot;
        return cc.done(failExprRootDiagnostic("borrowed binding: " + borrowRoot + " (root: " +
                                              borrowRoot + ", sink: " + borrowSink + ")"));
      }
    }
  }
  return PhaseStatus::Continue;
}

PhaseStatus SemanticsValidator::resolveCollectionTargets(ExprCallContext &cc) {
  const std::vector<ParameterInfo> &params = cc.params;
  const std::unordered_map<std::string, BindingInfo> &locals = cc.locals;
  const Expr &expr = cc.expr;
  [[maybe_unused]] auto &hasVectorHelperCallResolution = cc.hasVectorHelperCallResolution;
  [[maybe_unused]] auto &dispatchBootstrap = cc.dispatchBootstrap;
  [[maybe_unused]] auto &shouldBuiltinValidateBareKeyValueContainsCall =
      cc.shouldBuiltinValidateBareKeyValueContainsCall;
  [[maybe_unused]] auto &shouldBuiltinValidateBareKeyValueAccessCall =
      cc.shouldBuiltinValidateBareKeyValueAccessCall;
  [[maybe_unused]] auto &resolved = cc.resolved;
  [[maybe_unused]] auto &methodCompatibilitySetup = cc.methodCompatibilitySetup;
  [[maybe_unused]] auto &resolvedMethod = cc.resolvedMethod;
  [[maybe_unused]] auto &usedMethodTarget = cc.usedMethodTarget;
  [[maybe_unused]] auto &hasMethodReceiverIndex = cc.hasMethodReceiverIndex;
  [[maybe_unused]] auto &methodReceiverIndex = cc.methodReceiverIndex;
  [[maybe_unused]] auto &collectionDispatchSetup = cc.collectionDispatchSetup;
  [[maybe_unused]] auto &enclosingStatements = cc.enclosingStatements;
  [[maybe_unused]] auto &statementIndex = cc.statementIndex;
  [[maybe_unused]] auto &expressionIsStatementContext = cc.expressionIsStatementContext;
  [[maybe_unused]] auto publishExprRootDiagnostic = [&]() -> bool {
    captureExprContext(expr);
    return publishCurrentStructuredDiagnosticNow();
  };
  [[maybe_unused]] auto failExprRootDiagnostic = [&](std::string message) -> bool {
    return failExprDiagnostic(expr, std::move(message));
  };
  ExprCollectionCountCapacityDispatchContext collectionCountCapacityDispatchContext;
  collectionCountCapacityDispatchContext.isNamespacedVectorHelperCall =
      collectionDispatchSetup.isNamespacedVectorHelperCall;
  collectionCountCapacityDispatchContext.namespacedHelper =
      collectionDispatchSetup.namespacedHelper;
  collectionCountCapacityDispatchContext.isNamespacedVectorCapacityCall =
      collectionDispatchSetup.isNamespacedVectorCapacityCall;
  collectionCountCapacityDispatchContext.isDirectStdNamespacedVectorCountWrapperKeyValueTarget =
      collectionDispatchSetup.isDirectStdNamespacedVectorCountWrapperKeyValueTarget;
  collectionCountCapacityDispatchContext.resolveMapTarget = dispatchBootstrap.resolveMapTarget;
  collectionCountCapacityDispatchContext.isArrayNamespacedVectorCountCompatibilityCall =
      [&](const Expr &target) {
        return this->isArrayNamespacedVectorCountCompatibilityCall(
            target, dispatchBootstrap.dispatchResolvers);
      };
  collectionCountCapacityDispatchContext.tryRewriteBareVectorHelperCall =
      [&](const std::string &helperName, Expr &rewrittenOut) {
        return this->tryRewriteBareVectorHelperCall(
            expr, helperName, dispatchBootstrap.dispatchResolvers, rewrittenOut);
      };
  collectionCountCapacityDispatchContext.promoteCapacityToBuiltinValidation =
      methodCompatibilitySetup.promoteCapacityToBuiltinValidation;
  collectionCountCapacityDispatchContext.isNonCollectionStructCapacityTarget =
      methodCompatibilitySetup.isNonCollectionStructCapacityTarget;
  bool handledCollectionCountCapacityTarget = false;
  std::optional<Expr> rewrittenCollectionCountCapacityCall;
  if (!resolveExprCollectionCountCapacityTarget(params,
                                                locals,
                                                expr,
                                                collectionCountCapacityDispatchContext,
                                                handledCollectionCountCapacityTarget,
                                                rewrittenCollectionCountCapacityCall,
                                                resolved,
                                                resolvedMethod,
                                                usedMethodTarget,
                                                hasMethodReceiverIndex,
                                                methodReceiverIndex)) {
    return cc.done(false);
  }
  if (handledCollectionCountCapacityTarget && rewrittenCollectionCountCapacityCall.has_value()) {
    return cc.done(validateExpr(params, locals, *rewrittenCollectionCountCapacityCall));
  }
  std::optional<Expr> rewrittenDirectCollectionFallbackCall;
  if (!validateExprDirectCollectionFallbacks(params,
                                             locals,
                                             expr,
                                             resolved,
                                             dispatchBootstrap.dispatchResolvers,
                                             rewrittenDirectCollectionFallbackCall)) {
    return cc.done(false);
  }
  if (rewrittenDirectCollectionFallbackCall.has_value()) {
    // A rewritten mutator on an element of a builtin vector of vectors (`a[i].clear()`) or on a
    // vector field (`grid.rows.pop()`) stays a statement; elements of a variadic pack have no
    // in-place mutation.
    bool keepsStatementContext = false;
    if (const Expr &receiver = rewrittenDirectCollectionFallbackCall->args.empty()
                                   ? expr
                                   : rewrittenDirectCollectionFallbackCall->args.front();
        receiver.kind == Expr::Kind::Call &&
        ((receiver.isFieldAccess && receiver.args.size() == 1) ||
         (!receiver.isFieldAccess && receiver.args.size() == 2 &&
          (isSimpleCallName(receiver, "at") || isSimpleCallName(receiver, "at_unsafe"))))) {
      const Expr *root = containerPlaceRoot(receiver);
      const BindingInfo *rootBinding =
          root->kind == Expr::Kind::Name ? findParamBinding(params, root->name) : nullptr;
      if (rootBinding == nullptr && root->kind == Expr::Kind::Name) {
        if (const auto it = locals.find(root->name); it != locals.end()) {
          rootBinding = &it->second;
        }
      }
      keepsStatementContext = rootBinding != nullptr && rootBinding->typeName != "args";
    }
    if (!keepsStatementContext) {
      return cc.done(validateExpr(params, locals, *rewrittenDirectCollectionFallbackCall));
    }
    return cc.done(validateExpr(params,
                                locals,
                                *rewrittenDirectCollectionFallbackCall,
                                enclosingStatements,
                                statementIndex,
                                expressionIsStatementContext));
  }
  ExprCollectionAccessDispatchContext collectionAccessDispatchContext;
  prepareExprCollectionAccessDispatchContext(collectionDispatchSetup,
                                             shouldBuiltinValidateBareKeyValueContainsCall,
                                             shouldBuiltinValidateBareKeyValueAccessCall,
                                             dispatchBootstrap.dispatchResolvers,
                                             dispatchBootstrap.resolveMapTarget,
                                             collectionAccessDispatchContext);
  bool handledCollectionAccessTarget = false;
  if (!resolveExprCollectionAccessTarget(params,
                                         locals,
                                         expr,
                                         collectionAccessDispatchContext,
                                         handledCollectionAccessTarget,
                                         resolved,
                                         resolvedMethod,
                                         usedMethodTarget,
                                         hasMethodReceiverIndex,
                                         methodReceiverIndex)) {
    return cc.done(false);
  }
  bool handledPostAccessPrecheck = false;
  if (!validateExprPostAccessPrechecks(params,
                                       locals,
                                       expr,
                                       resolved,
                                       resolvedMethod,
                                       usedMethodTarget,
                                       dispatchBootstrap.dispatchResolverAdapters,
                                       enclosingStatements,
                                       statementIndex,
                                       handledPostAccessPrecheck)) {
    return cc.done(false);
  }
  if (handledPostAccessPrecheck) {
    return cc.done(true);
  }
  ExprNamedArgumentBuiltinContext namedArgumentBuiltinContext;
  prepareExprNamedArgumentBuiltinContext(hasVectorHelperCallResolution,
                                         dispatchBootstrap.dispatchResolvers,
                                         namedArgumentBuiltinContext);
  if (!validateExprNamedArguments(
          params, locals, expr, resolved, resolvedMethod, namedArgumentBuiltinContext)) {
    if (error_.empty()) {
      return cc.done(failExprRootDiagnostic("validateExprNamedArguments failed"));
    }
    return cc.done(false);
  }
  auto validateResolvedCanonicalKeyValueAccessKey = [&]() -> bool {
    if (expr.isMethodCall || expr.args.size() != 2) {
      return true;
    }
    const StdlibSurfaceMetadata *metadata = keyValueHelperSurfaceMetadataLocal();
    std::string helperName;
    if (metadata == nullptr ||
        !resolvePublishedCollectionHelperResolvedPath(resolved, metadata->id, helperName)) {
      return true;
    }
    if (!collection_helpers::isTryAtHelperName(helperName) &&
        !collection_helpers::isAtHelperName(helperName) &&
        !collection_helpers::isAtUnsafeHelperName(helperName)) {
      return true;
    }
    std::string keyValueKeyType;
    if (!resolveKeyValueKeyType(
            expr.args.front(), dispatchBootstrap.dispatchResolvers, keyValueKeyType)) {
      std::string receiverTypeText;
      std::string keyValueValueType;
      if (!inferQueryExprTypeText(expr.args.front(), params, locals, receiverTypeText) ||
          !extractKeyValueCollectionTypesFromTypeText(
              receiverTypeText, keyValueKeyType, keyValueValueType)) {
        return true;
      }
    }
    auto failKeyValueKeyDiagnostic = [&]() {
      if (collection_helpers::isTryAtHelperName(helperName)) {
        if (normalizeBindingTypeName(keyValueKeyType) == "string") {
          return failExprDiagnostic(expr.args[1], "tryAt requires string map key");
        }
        return failExprDiagnostic(expr.args[1], "tryAt requires map key type " + keyValueKeyType);
      }
      return failExprDiagnostic(expr.args[1],
                                "argument type mismatch for " + resolved + " parameter key");
    };
    if (normalizeBindingTypeName(keyValueKeyType) == "string") {
      if (!isStringExprForArgumentValidation(expr.args[1], dispatchBootstrap.dispatchResolvers)) {
        return failKeyValueKeyDiagnostic();
      }
      return true;
    }
    const ReturnKind keyKind = returnKindForTypeName(normalizeBindingTypeName(keyValueKeyType));
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
    return cc.done(false);
  }
  return PhaseStatus::Continue;
}

} // namespace primec::semantics
