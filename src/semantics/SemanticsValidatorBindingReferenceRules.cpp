#include "SemanticsValidator.h"

#include "SemanticsValidatorInferCollectionCompatibilityInternal.h"

#include "SemanticsValidatorStatementBindingsHelpers.h"
#include "SemanticsValidatorStatementBindingsState.h"
#include "SemanticsValidatorBindingReferenceContext.h"

// Validation of a `Reference<T>` binding as an ordered list of rules (TODO-5981). A rule returns Done when it has
// decided the binding (accepted or rejected) and Continue to leave it to the next one; the helpers they use are
// in SemanticsValidatorBindingReferenceResolver.cpp.

namespace primec::semantics {
using namespace statementBindingsHelpers;

// Decides what the initializer is: a `location(...)`, a direct `borrow(...)` of storage, a standalone `ref(...)`
// borrow, or (in an unsafe body) a pointer conversion. The first three kinds of rejection and both of the early
// acceptances happen here; a `location(...)` or direct borrow continues to `bindReferenceRoot`.
PhaseStatus SemanticsValidator::classifyReferenceInitializer(BindingReferenceContext &ctx) {
  std::unordered_map<std::string, BindingInfo> &locals = ctx.locals;
  const Expr &stmt = ctx.stmt;
  const std::string &namespacePrefix = ctx.namespacePrefix;
  ValidateBindingState &st = ctx.st;
  auto &failBindingDiagnostic = st.failBindingDiagnostic;
  auto &definitionTemplateArgs = st.definitionTemplateArgs;
  auto &info = st.info;
  auto &resolvePointerRoot = st.resolvePointerRoot;
  const Expr &init = ctx.init;
  std::string pointerName;
  const bool initIsLocation = init.kind == Expr::Kind::Call &&
                              getBuiltinPointerName(init, pointerName) &&
                              pointerName == "location" && init.args.size() == 1;
  std::string safeTargetType;
  const bool initIsDirectBorrowStorage =
      resolveDirectBorrowStorageTargetType(ctx, init, safeTargetType);
  const bool initIsPointerLike = resolvePointerTargetType(ctx, init, safeTargetType);
  const bool initIsBorrowedFieldAccess = init.kind == Expr::Kind::Call && init.isFieldAccess &&
                                         init.args.size() == 1 && initIsPointerLike;
  if (!initIsLocation && !initIsDirectBorrowStorage && !initIsPointerLike &&
      !currentValidationState_.context.definitionIsUnsafe) {
    return ctx.done(st.done(failBindingDiagnostic("Reference bindings require location(...)")));
  }
  if (initIsLocation || initIsDirectBorrowStorage ||
      (!currentValidationState_.context.definitionIsUnsafe && initIsPointerLike)) {
    if (!errorTypesMatch(safeTargetType, info.typeTemplateArg, namespacePrefix)) {
      return ctx.done(st.done(failBindingDiagnostic("Reference binding type mismatch")));
    }
  }
  if (!initIsLocation && !initIsDirectBorrowStorage && !initIsBorrowedFieldAccess &&
      !currentValidationState_.context.definitionIsUnsafe) {
    std::string borrowRoot;
    const ExprSubstitutions substitutions;
    const bool resolvedStandaloneRoot =
        resolveStandaloneRefRootExpr(ctx, init, substitutions, borrowRoot);
    if (isStandaloneRefCall(ctx, init) && (!resolvedStandaloneRoot || borrowRoot.empty())) {
      return ctx.done(st.done(failBindingDiagnostic("Reference binding requires borrow root")));
    }
    if (resolvedStandaloneRoot && !borrowRoot.empty()) {
      if (hasBorrowConflictForRoot(ctx, borrowRoot, info.isMutable)) {
        return ctx.done(
            st.done(failBindingDiagnostic("borrow conflict: " + borrowRoot +
                                          " (root: " + borrowRoot + ", sink: " + stmt.name + ")")));
      }
      if (info.isMutable && !isMutableRootBinding(ctx, borrowRoot)) {
        return ctx.done(st.done(
            failBindingDiagnostic("Reference binding requires mutable root: " + borrowRoot)));
      }
      info.referenceRoot = std::move(borrowRoot);
    }
    if (!validateBuiltinComparableKeyType(info, definitionTemplateArgs, error_)) {
      return ctx.done(st.done(false));
    }
    insertLocalBinding(locals, stmt.name, std::move(info));
    return ctx.done(st.done(true));
  }
  // `borrow(...)` takes the ordinary borrow path in unsafe bodies too; other non-`location`
  // initializers there are pointer conversions.
  if (!initIsLocation && !initIsDirectBorrowStorage &&
      currentValidationState_.context.definitionIsUnsafe) {
    std::string pointerTargetType;
    if (!resolvePointerTargetType(ctx, init, pointerTargetType)) {
      return ctx.done(st.done(
          failBindingDiagnostic("unsafe Reference bindings require pointer-like initializer")));
    }
    if (!errorTypesMatch(pointerTargetType, info.typeTemplateArg, namespacePrefix)) {
      return ctx.done(st.done(failBindingDiagnostic("unsafe Reference binding type mismatch")));
    }
    std::string borrowRoot;
    if (resolvePointerRoot(init, borrowRoot)) {
      info.referenceRoot = std::move(borrowRoot);
    }
    info.isUnsafeReference = true;
    if (!validateBuiltinComparableKeyType(info, definitionTemplateArgs, error_)) {
      return ctx.done(st.done(false));
    }
    insertLocalBinding(locals, stmt.name, std::move(info));
    return ctx.done(st.done(true));
  }
  ctx.target = &((initIsLocation || initIsDirectBorrowStorage) ? init.args.front() : init);
  return PhaseStatus::Continue;
}

// Finds the root a `location(...)`/direct borrow borrows from, rejects it when another borrow of that root
// conflicts and records the root on the binding.
PhaseStatus SemanticsValidator::bindReferenceRoot(BindingReferenceContext &ctx) {
  const std::vector<ParameterInfo> &params = ctx.params;
  std::unordered_map<std::string, BindingInfo> &locals = ctx.locals;
  const Expr &stmt = ctx.stmt;
  ValidateBindingState &st = ctx.st;
  auto &failBindingDiagnostic = st.failBindingDiagnostic;
  auto &info = st.info;
  const Expr &target = *ctx.target;
  std::string borrowRoot;
  if (!resolveBorrowRootExpr(ctx, target, borrowRoot) || borrowRoot.empty()) {
    return ctx.done(st.done(failBindingDiagnostic("Reference bindings require location(...)")));
  }
  bool sawMutableBorrow = false;
  bool sawImmutableBorrow = false;
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
  const bool conflict =
      info.isMutable ? (sawMutableBorrow || sawImmutableBorrow) : sawMutableBorrow;
  if (conflict && !currentValidationState_.context.definitionIsUnsafe) {
    return ctx.done(st.done(failBindingDiagnostic("borrow conflict: " + borrowRoot + " (root: " +
                                                  borrowRoot + ", sink: " + stmt.name + ")")));
  }
  info.referenceRoot = std::move(borrowRoot);
  info.isUnsafeReference = currentValidationState_.context.definitionIsUnsafe;
  return PhaseStatus::Continue;
}

PhaseStatus
SemanticsValidator::validateBindingReference(const std::vector<ParameterInfo> &params,
                                             std::unordered_map<std::string, BindingInfo> &locals,
                                             const Expr &stmt,
                                             const std::string &namespacePrefix,
                                             ValidateBindingState &st) {
  BindingReferenceContext ctx{params, locals, stmt, namespacePrefix, st, *st.initializer};
  using Rule = PhaseStatus (SemanticsValidator::*)(BindingReferenceContext &);
  static constexpr Rule Rules[] = {&SemanticsValidator::classifyReferenceInitializer,
                                   &SemanticsValidator::bindReferenceRoot};
  for (const Rule rule : Rules) {
    if ((this->*rule)(ctx) == PhaseStatus::Done) {
      return ctx.result;
    }
  }
  return PhaseStatus::Continue;
}

} // namespace primec::semantics
