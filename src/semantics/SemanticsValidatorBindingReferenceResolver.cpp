#include "SemanticsValidator.h"

#include "SemanticsValidatorInferCollectionCompatibilityInternal.h"

#include <algorithm>
#include <optional>
#include "primec/ir/StdlibCollectionPaths.h"
#include "primec/support/CollectionHelperNames.h"
#include "SemanticsValidatorStatementBindingsHelpers.h"
#include "SemanticsValidatorBindingContext.h"
#include "SemanticsValidatorBindingReferenceContext.h"

// The helpers of a `Reference<T>` binding: what a borrow targets, which root it borrows from and whether that root
// conflicts with another borrow (TODO-5981). Each takes the binding's context explicitly; they were lambdas of
// `validateBindingPhase5` kept in a state struct of `std::function` members.

namespace primec::semantics {
using namespace statementBindingsHelpers;

namespace {

std::string formatBindingType(const BindingInfo &binding) {
  if (binding.typeTemplateArg.empty()) {
    return binding.typeName;
  }
  return binding.typeName + "<" + binding.typeTemplateArg + ">";
}

bool isStandaloneBorrowStorageExpr(const Expr &candidate) {
  if (candidate.kind == Expr::Kind::Name) {
    return true;
  }
  std::string builtinName;
  if (candidate.kind == Expr::Kind::Call && getBuiltinPointerName(candidate, builtinName) &&
      builtinName == "dereference" && candidate.args.size() == 1) {
    return true;
  }
  return candidate.kind == Expr::Kind::Call && candidate.isFieldAccess &&
         candidate.args.size() == 1;
}

} // namespace

bool SemanticsValidator::resolveDirectBorrowStorageTargetType(BindingReferenceContext &ctx,
                                                              const Expr &expr,
                                                              std::string &targetOut) {
  const std::vector<ParameterInfo> &params = ctx.params;
  std::unordered_map<std::string, BindingInfo> &locals = ctx.locals;

  if (expr.kind != Expr::Kind::Call || expr.isMethodCall || !isSimpleCallName(expr, "borrow") ||
      expr.args.size() != 1) {
    return false;
  }
  const Expr &storage = expr.args.front();
  if (!isStandaloneBorrowStorageExpr(storage)) {
    return false;
  }
  BindingInfo binding;
  bool resolved = false;
  if (!resolveUninitializedStorageBinding(params, locals, storage, binding, resolved)) {
    return false;
  }
  if (!resolved || binding.typeName != "uninitialized" || binding.typeTemplateArg.empty()) {
    return false;
  }
  targetOut = binding.typeTemplateArg;
  return true;
}

bool SemanticsValidator::resolvePointerTargetType(BindingReferenceContext &ctx,
                                                  const Expr &expr,
                                                  std::string &targetOut) {
  const std::vector<ParameterInfo> &params = ctx.params;
  std::unordered_map<std::string, BindingInfo> &locals = ctx.locals;

  if (expr.kind == Expr::Kind::Name) {
    const BindingInfo *binding = resolveNamedBinding(ctx.st, expr.name);
    if (binding == nullptr) {
      return false;
    }
    if ((binding->typeName == "Pointer" || binding->typeName == "Reference") &&
        !binding->typeTemplateArg.empty()) {
      targetOut = binding->typeTemplateArg;
      return true;
    }
    return false;
  }
  if (expr.kind != Expr::Kind::Call) {
    return false;
  }
  std::string builtinName;
  if (getBuiltinPointerName(expr, builtinName) && builtinName == "location" &&
      expr.args.size() == 1) {
    const Expr &target = expr.args.front();
    if (target.kind == Expr::Kind::Name) {
      const BindingInfo *binding = resolveNamedBinding(ctx.st, target.name);
      if (binding == nullptr) {
        return false;
      }
      if (binding->typeName == "Reference" && !binding->typeTemplateArg.empty()) {
        targetOut = binding->typeTemplateArg;
      } else {
        targetOut = formatBindingType(*binding);
      }
      return true;
    }
    BindingInfo inferredBinding;
    if (!inferBindingTypeFromInitializer(target, params, locals, inferredBinding)) {
      return false;
    }
    if (inferredBinding.typeName == "Reference" && !inferredBinding.typeTemplateArg.empty()) {
      targetOut = inferredBinding.typeTemplateArg;
    } else {
      targetOut = formatBindingType(inferredBinding);
    }
    return true;
  }
  if (expr.kind == Expr::Kind::Call && expr.isFieldAccess && expr.args.size() == 1) {
    std::string receiverTargetType;
    if (!resolvePointerTargetType(ctx, expr.args.front(), receiverTargetType)) {
      return false;
    }
    BindingInfo inferredBinding;
    if (!inferBindingTypeFromInitializer(expr, params, locals, inferredBinding)) {
      return false;
    }
    if (inferredBinding.typeName == "Reference" && !inferredBinding.typeTemplateArg.empty()) {
      targetOut = inferredBinding.typeTemplateArg;
    } else {
      targetOut = formatBindingType(inferredBinding);
    }
    return !targetOut.empty();
  }
  std::string opName;
  if (getBuiltinOperatorName(expr, opName) && (opName == "plus" || opName == "minus") &&
      expr.args.size() == 2) {
    if (isPointerLikeExpr(expr.args[1], params, locals)) {
      return false;
    }
    return resolvePointerTargetType(ctx, expr.args[0], targetOut);
  }
  auto resolveImplicitSoaRefTargetType = [&](std::string &targetOut) -> bool {
    if (expr.args.size() != 2) {
      return false;
    }
    const std::string normalizedName =
        !expr.name.empty() && expr.name.front() == '/' ? expr.name.substr(1) : expr.name;
    std::string resolvedPath = preferredCollectionHelperResolvedPath(expr);
    if (resolvedPath.empty()) {
      resolvedPath = resolveCalleePath(expr);
    }
    if (const std::string concreteResolvedPath =
            resolveExprConcreteCallPath(params, locals, expr, resolvedPath);
        !concreteResolvedPath.empty()) {
      resolvedPath = concreteResolvedPath;
    }
    const std::string resolvedPathCanonical = canonicalizeLegacySoaRefHelperPath(resolvedPath);
    const bool resolvedCanonicalRefLike = isCanonicalSoaRefLikeHelperPath(resolvedPathCanonical);
    const bool resolvedExperimentalRefLike =
        isExperimentalSoaRefLikeHelperPath(resolvedPathCanonical);
    const auto soaAccessHelper = builtinSoaAccessHelperName(expr, params, locals);
    const bool helperResolvedRefLike =
        soaAccessHelper.has_value() &&
        (*soaAccessHelper == "ref" || *soaAccessHelper == collection_helpers::kRefRef);
    const bool isMethodRefLike =
        expr.isMethodCall &&
        (collection_helpers::isRefHelperName(normalizedName) || helperResolvedRefLike ||
         resolvedCanonicalRefLike || resolvedExperimentalRefLike);
    const bool isHelperRefLike =
        !expr.isMethodCall &&
        (isSimpleCallName(expr, "ref") || isSimpleCallName(expr, collection_helpers::kRefRef) ||
         helperResolvedRefLike || resolvedCanonicalRefLike || resolvedExperimentalRefLike);
    if (!isMethodRefLike && !isHelperRefLike) {
      return false;
    }
    std::string elemType;
    const Expr &receiver = expr.args.front();
    if (receiver.kind == Expr::Kind::Name) {
      const BindingInfo *binding = resolveNamedBinding(ctx.st, receiver.name);
      if (binding == nullptr || !extractExperimentalSoaVectorElementType(*binding, elemType)) {
        return false;
      }
      targetOut = elemType;
      return true;
    }
    BindingInfo receiverBinding;
    std::string receiverTypeText;
    if (!resolvePointerTargetType(ctx, receiver, receiverTypeText) &&
        !inferQueryExprTypeText(receiver, params, locals, receiverTypeText)) {
      return false;
    }
    std::string base;
    std::string argText;
    const std::string normalizedType = normalizeBindingTypeName(receiverTypeText);
    if (splitTemplateTypeName(normalizedType, base, argText)) {
      receiverBinding.typeName = normalizeBindingTypeName(base);
      receiverBinding.typeTemplateArg = argText;
    } else {
      receiverBinding.typeName = normalizedType;
      receiverBinding.typeTemplateArg.clear();
    }
    if (!extractExperimentalSoaVectorElementType(receiverBinding, elemType)) {
      return false;
    }
    targetOut = elemType;
    return true;
  };
  if (resolveImplicitSoaRefTargetType(targetOut)) {
    return true;
  }
  std::string resolvedPath = preferredCollectionHelperResolvedPath(expr);
  if (resolvedPath.empty()) {
    resolvedPath = resolveCalleePath(expr);
  }
  if (expr.isMethodCall) {
    if (expr.args.empty()) {
      return false;
    }
    bool isBuiltin = false;
    if (!resolveMethodTarget(params,
                             locals,
                             expr.namespacePrefix,
                             expr.args.front(),
                             expr.name,
                             resolvedPath,
                             isBuiltin)) {
      return false;
    }
  }
  if (const std::string concreteResolvedPath =
          resolveExprConcreteCallPath(params, locals, expr, resolvedPath);
      !concreteResolvedPath.empty()) {
    resolvedPath = concreteResolvedPath;
  }
  auto defIt = defMap_.find(resolvedPath);
  if (defIt == defMap_.end() || defIt->second == nullptr) {
    return false;
  }
  for (const auto &transform : defIt->second->transforms) {
    if (transform.name != "return" || transform.templateArgs.size() != 1) {
      continue;
    }
    std::string base;
    std::string arg;
    if (!splitTemplateTypeName(transform.templateArgs.front(), base, arg)) {
      continue;
    }
    if (base != "Reference" && base != "Pointer") {
      continue;
    }
    std::vector<std::string> args;
    if (!splitTopLevelTemplateArgs(arg, args) || args.size() != 1) {
      return false;
    }
    targetOut = args.front();
    return true;
  }
  return false;
}

bool SemanticsValidator::resolveBorrowRoot(BindingReferenceContext &ctx,
                                           const std::string &targetName,
                                           std::string &rootOut) {
  const std::vector<ParameterInfo> &params = ctx.params;
  std::unordered_map<std::string, BindingInfo> &locals = ctx.locals;

  if (const BindingInfo *paramBinding = findParamBinding(params, targetName)) {
    if (paramBinding->typeName == "Reference" || isSoaFieldViewBindingType(*paramBinding)) {
      rootOut = referenceRootForBorrowBinding(targetName, *paramBinding);
    } else {
      rootOut = targetName;
    }
    return true;
  }
  auto it = locals.find(targetName);
  if (it == locals.end()) {
    return false;
  }
  if (it->second.typeName == "Reference" || isSoaFieldViewBindingType(it->second)) {
    rootOut = referenceRootForBorrowBinding(it->first, it->second);
  } else {
    rootOut = targetName;
  }
  return true;
}

static const Expr *findSubstitutedExpr(const ExprSubstitutions &substitutions,
                                       const std::string &name,
                                       size_t *matchedIndexOut) {

  for (size_t index = substitutions.size(); index > 0; --index) {
    if (substitutions[index - 1].first == name) {
      if (matchedIndexOut != nullptr) {
        *matchedIndexOut = index - 1;
      }
      return substitutions[index - 1].second;
    }
  }
  return nullptr;
}

static ExprSubstitutions removeSubstitutionAt(const ExprSubstitutions &substitutions,
                                              size_t indexToSkip) {

  ExprSubstitutions reduced;
  reduced.reserve(substitutions.size());
  for (size_t index = 0; index < substitutions.size(); ++index) {
    if (index == indexToSkip) {
      continue;
    }
    reduced.push_back(substitutions[index]);
  }
  return reduced;
}

bool SemanticsValidator::appendCallSubstitutions(BindingReferenceContext &ctx,
                                                 const Expr &callExpr,
                                                 const ExprSubstitutions &baseSubstitutions,
                                                 ExprSubstitutions &extendedSubstitutions,
                                                 const Expr *&returnedValueExprOut) {
  const std::vector<ParameterInfo> &params = ctx.params;
  std::unordered_map<std::string, BindingInfo> &locals = ctx.locals;

  returnedValueExprOut = nullptr;
  std::string resolvedCallPath = preferredCollectionHelperResolvedPath(callExpr);
  if (resolvedCallPath.empty()) {
    resolvedCallPath = resolveCalleePath(callExpr);
  }
  if (callExpr.isMethodCall) {
    if (callExpr.args.empty()) {
      return false;
    }
    bool isBuiltin = false;
    if (!resolveMethodTarget(params,
                             locals,
                             callExpr.namespacePrefix,
                             callExpr.args.front(),
                             callExpr.name,
                             resolvedCallPath,
                             isBuiltin)) {
      return false;
    }
  }
  if (const std::string concreteResolvedCallPath =
          resolveExprConcreteCallPath(params, locals, callExpr, resolvedCallPath);
      !concreteResolvedCallPath.empty()) {
    resolvedCallPath = concreteResolvedCallPath;
  }
  auto defIt = defMap_.find(resolvedCallPath);
  if (defIt == defMap_.end() || defIt->second == nullptr) {
    return false;
  }
  const auto paramsIt = paramsByDef_.find(resolvedCallPath);
  if (paramsIt == paramsByDef_.end()) {
    return false;
  }
  const auto &nestedParams = paramsIt->second;
  std::string nestedArgError;
  std::vector<const Expr *> nestedOrderedArgs;
  if (!buildOrderedArguments(
          nestedParams, callExpr.args, callExpr.argNames, nestedOrderedArgs, nestedArgError)) {
    return false;
  }
  const Definition &nestedDef = *defIt->second;
  for (const auto &stmtExpr : nestedDef.statements) {
    if (isReturnCall(stmtExpr) && stmtExpr.args.size() == 1) {
      returnedValueExprOut = &stmtExpr.args.front();
    }
  }
  if (nestedDef.returnExpr.has_value()) {
    returnedValueExprOut = &*nestedDef.returnExpr;
  }
  if (returnedValueExprOut == nullptr) {
    return false;
  }
  extendedSubstitutions = baseSubstitutions;
  for (size_t nestedIndex = 0;
       nestedIndex < nestedParams.size() && nestedIndex < nestedOrderedArgs.size();
       ++nestedIndex) {
    const Expr *nestedArg = nestedOrderedArgs[nestedIndex];
    if (nestedArg == nullptr) {
      continue;
    }
    extendedSubstitutions.emplace_back(nestedParams[nestedIndex].name, nestedArg);
  }
  return true;
}

bool SemanticsValidator::resolveConcreteCallPath(BindingReferenceContext &ctx,
                                                 const Expr &callExpr,
                                                 std::string &resolvedPathOut) {
  const std::vector<ParameterInfo> &params = ctx.params;
  std::unordered_map<std::string, BindingInfo> &locals = ctx.locals;

  resolvedPathOut = preferredCollectionHelperResolvedPath(callExpr);
  if (resolvedPathOut.empty()) {
    resolvedPathOut = resolveCalleePath(callExpr);
  }
  if (callExpr.kind != Expr::Kind::Call) {
    return !resolvedPathOut.empty();
  }
  if (callExpr.isMethodCall) {
    if (callExpr.args.empty()) {
      return false;
    }
    bool isBuiltin = false;
    if (!resolveMethodTarget(params,
                             locals,
                             callExpr.namespacePrefix,
                             callExpr.args.front(),
                             callExpr.name,
                             resolvedPathOut,
                             isBuiltin)) {
      return false;
    }
  }
  if (const std::string concreteResolvedPathOut =
          resolveExprConcreteCallPath(params, locals, callExpr, resolvedPathOut);
      !concreteResolvedPathOut.empty()) {
    resolvedPathOut = concreteResolvedPathOut;
  }
  return !resolvedPathOut.empty();
}

bool SemanticsValidator::resolveReceiverRootExpr(BindingReferenceContext &ctx,
                                                 const Expr &expr,
                                                 const ExprSubstitutions &substitutions,
                                                 std::string &rootOut) {

  if (expr.kind == Expr::Kind::Name) {
    size_t matchedIndex = 0;
    if (const Expr *substitutedExpr =
            findSubstitutedExpr(substitutions, expr.name, &matchedIndex)) {
      const ExprSubstitutions reducedSubstitutions =
          removeSubstitutionAt(substitutions, matchedIndex);
      return resolveReceiverRootExpr(ctx, *substitutedExpr, reducedSubstitutions, rootOut);
    }
    return resolveBorrowRoot(ctx, expr.name, rootOut);
  }
  if (expr.kind != Expr::Kind::Call) {
    return false;
  }
  std::string builtinName;
  if (getBuiltinPointerName(expr, builtinName) && expr.args.size() == 1) {
    if (builtinName == "location" || builtinName == "dereference") {
      return resolveReceiverRootExpr(ctx, expr.args.front(), substitutions, rootOut);
    }
  }
  ExprSubstitutions nestedSubstitutions;
  const Expr *returnedValueExpr = nullptr;
  if (!appendCallSubstitutions(ctx, expr, substitutions, nestedSubstitutions, returnedValueExpr)) {
    return false;
  }
  return resolveReceiverRootExpr(ctx, *returnedValueExpr, nestedSubstitutions, rootOut);
}

bool SemanticsValidator::resolveStandaloneRefRootExpr(BindingReferenceContext &ctx,
                                                      const Expr &expr,
                                                      const ExprSubstitutions &substitutions,
                                                      std::string &rootOut) {

  if (expr.kind == Expr::Kind::Name) {
    size_t matchedIndex = 0;
    if (const Expr *substitutedExpr =
            findSubstitutedExpr(substitutions, expr.name, &matchedIndex)) {
      const ExprSubstitutions reducedSubstitutions =
          removeSubstitutionAt(substitutions, matchedIndex);
      return resolveStandaloneRefRootExpr(ctx, *substitutedExpr, reducedSubstitutions, rootOut);
    }
    return false;
  }
  if (expr.kind == Expr::Kind::Call && expr.args.size() == 2) {
    std::string resolvedPath = preferredCollectionHelperResolvedPath(expr);
    if (resolvedPath.empty()) {
      resolvedPath = resolveCalleePath(expr);
    }
    (void)resolveConcreteCallPath(ctx, expr, resolvedPath);
    const std::string resolvedPathCanonical = canonicalizeLegacySoaRefHelperPath(resolvedPath);
    const bool resolvedCanonicalRefLike = isCanonicalSoaRefLikeHelperPath(resolvedPathCanonical);
    const bool resolvedExperimentalRefLike =
        isExperimentalSoaRefLikeHelperPath(resolvedPathCanonical);
    const bool isMethodRefCall =
        expr.isMethodCall &&
        (expr.name == "ref" || resolvedCanonicalRefLike || resolvedExperimentalRefLike);
    const bool isHelperRefCall =
        !expr.isMethodCall &&
        (isSimpleCallName(expr, "ref") || resolvedCanonicalRefLike || resolvedExperimentalRefLike);
    if ((isMethodRefCall || isHelperRefCall) &&
        resolveReceiverRootExpr(ctx, expr.args.front(), substitutions, rootOut)) {
      return !rootOut.empty();
    }
  }
  if (expr.kind != Expr::Kind::Call) {
    return false;
  }
  ExprSubstitutions nestedSubstitutions;
  const Expr *returnedValueExpr = nullptr;
  if (!appendCallSubstitutions(ctx, expr, substitutions, nestedSubstitutions, returnedValueExpr)) {
    return false;
  }
  return resolveStandaloneRefRootExpr(ctx, *returnedValueExpr, nestedSubstitutions, rootOut);
}

bool SemanticsValidator::isStandaloneRefCall(BindingReferenceContext &ctx, const Expr &expr) {

  if (expr.kind != Expr::Kind::Call || expr.args.size() != 2) {
    return false;
  }
  std::string resolvedPath = preferredCollectionHelperResolvedPath(expr);
  if (resolvedPath.empty()) {
    resolvedPath = resolveCalleePath(expr);
  }
  (void)resolveConcreteCallPath(ctx, expr, resolvedPath);
  const std::string resolvedPathCanonical = canonicalizeLegacySoaRefHelperPath(resolvedPath);
  const bool resolvedCanonicalRefLike = isCanonicalSoaRefLikeHelperPath(resolvedPathCanonical);
  const bool resolvedExperimentalRefLike =
      isExperimentalSoaRefLikeHelperPath(resolvedPathCanonical);
  if (expr.isMethodCall) {
    return expr.name == "ref" || resolvedCanonicalRefLike || resolvedExperimentalRefLike;
  }
  return isSimpleCallName(expr, "ref") || resolvedCanonicalRefLike || resolvedExperimentalRefLike;
}

bool SemanticsValidator::hasBorrowConflictForRoot(BindingReferenceContext &ctx,
                                                  const std::string &borrowRoot,
                                                  bool requestMutable) {
  const std::vector<ParameterInfo> &params = ctx.params;
  std::unordered_map<std::string, BindingInfo> &locals = ctx.locals;

  if (borrowRoot.empty() || currentValidationState_.context.definitionIsUnsafe) {
    return false;
  }
  bool sawMutableBorrow = false;
  bool sawImmutableBorrow = false;
  auto referenceRootForBorrowBinding = [&](const std::string &bindingName,
                                           const BindingInfo &binding) -> std::string {
    if (!isBorrowTrackedBindingType(binding)) {
      return "";
    }
    if (!binding.referenceRoot.empty()) {
      return binding.referenceRoot;
    }
    return bindingName;
  };
  auto observeBorrow = [&](const std::string &bindingName, const BindingInfo &binding) {
    if (currentValidationState_.endedReferenceBorrows.count(bindingName) > 0) {
      return;
    }
    const std::string root = referenceRootForBorrowBinding(bindingName, binding);
    if (root.empty() || root != borrowRoot) {
      return;
    }
    if (binding.isMutable) {
      sawMutableBorrow = true;
    } else {
      sawImmutableBorrow = true;
    }
  };
  for (const auto &param : params) {
    observeBorrow(param.name, param.binding);
  }
  for (const auto &entry : locals) {
    observeBorrow(entry.first, entry.second);
  }
  return requestMutable ? (sawMutableBorrow || sawImmutableBorrow) : sawMutableBorrow;
}

bool SemanticsValidator::isMutableRootBinding(BindingReferenceContext &ctx,
                                              const std::string &borrowRoot) {
  const std::vector<ParameterInfo> &params = ctx.params;
  std::unordered_map<std::string, BindingInfo> &locals = ctx.locals;

  if (borrowRoot.empty()) {
    return false;
  }
  if (const BindingInfo *paramBinding = findParamBinding(params, borrowRoot)) {
    return paramBinding->isMutable;
  }
  auto it = locals.find(borrowRoot);
  return it != locals.end() && it->second.isMutable;
}

bool SemanticsValidator::resolveBorrowRootExpr(BindingReferenceContext &ctx,
                                               const Expr &targetExpr,
                                               std::string &rootOut) {

  if (targetExpr.kind == Expr::Kind::Name) {
    return resolveBorrowRoot(ctx, targetExpr.name, rootOut);
  }
  std::string builtinName;
  if (targetExpr.kind == Expr::Kind::Call && getBuiltinPointerName(targetExpr, builtinName) &&
      builtinName == "dereference" && targetExpr.args.size() == 1) {
    return resolvePointerRoot(ctx.st, targetExpr.args.front(), rootOut);
  }
  if (targetExpr.kind == Expr::Kind::Call && targetExpr.isFieldAccess &&
      targetExpr.args.size() == 1) {
    std::string receiverRoot;
    if (!resolveBorrowRootExpr(ctx, targetExpr.args.front(), receiverRoot) ||
        receiverRoot.empty()) {
      return false;
    }
    rootOut = receiverRoot + "." + targetExpr.name;
    return true;
  }
  return false;
}

} // namespace primec::semantics
