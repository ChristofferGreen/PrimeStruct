#include "SemanticsValidator.h"

#include "SemanticsValidatorInferCollectionCompatibilityInternal.h"

#include <algorithm>
#include <functional>
#include <optional>
#include <unordered_set>
#include "primec/ir/StdlibCollectionPaths.h"
#include "primec/support/CollectionHelperNames.h"
#include "SemanticsValidatorStatementBindingsHelpers.h"
#include "SemanticsValidatorStatementBindingsState.h"
#include "SemanticsValidatorStatementBindingsReferenceState.h"

namespace primec::semantics {
using namespace statementBindingsHelpers;

PhaseStatus SemanticsValidator::validateBindingReferencePhase4([[maybe_unused]] const std::vector<ParameterInfo> &params, [[maybe_unused]] std::unordered_map<std::string, BindingInfo> &locals, [[maybe_unused]] const Expr &stmt, [[maybe_unused]] const std::string &namespacePrefix, [[maybe_unused]] bool &handled, [[maybe_unused]] ValidateBindingState &st, ValidateBindingReferenceState &st2) {
  [[maybe_unused]] auto &allowBindings = st.allowBindings;
  [[maybe_unused]] auto &allowCompileTimeTypeBindings = st.allowCompileTimeTypeBindings;
  [[maybe_unused]] auto &failBindingDiagnostic = st.failBindingDiagnostic;
  [[maybe_unused]] auto &definitionTemplateArgs = st.definitionTemplateArgs;
  [[maybe_unused]] auto &info = st.info;
  [[maybe_unused]] const Expr &initializer = *st.initializer;
  [[maybe_unused]] auto &resolveNamedBinding = st.resolveNamedBinding;
  [[maybe_unused]] auto &resolvePointerRoot = st.resolvePointerRoot;
  [[maybe_unused]] auto &resolveBorrowRoot = st2.resolveBorrowRoot;
  [[maybe_unused]] auto &findSubstitutedExpr = st2.findSubstitutedExpr;
  [[maybe_unused]] auto &removeSubstitutionAt = st2.removeSubstitutionAt;
  [[maybe_unused]] auto &appendCallSubstitutions = st2.appendCallSubstitutions;
  [[maybe_unused]] auto &resolveConcreteCallPath = st2.resolveConcreteCallPath;
  [[maybe_unused]] auto &resolveReceiverRootExpr = st2.resolveReceiverRootExpr;
  [[maybe_unused]] auto &resolveStandaloneRefRootExpr = st2.resolveStandaloneRefRootExpr;
  st2.isMutableRootBinding = [&](const std::string &borrowRoot) -> bool {
      if (borrowRoot.empty()) {
        return false;
      }
      if (const BindingInfo *paramBinding = findParamBinding(params, borrowRoot)) {
        return paramBinding->isMutable;
      }
      auto it = locals.find(borrowRoot);
      return it != locals.end() && it->second.isMutable;
    };
  [[maybe_unused]] auto &isMutableRootBinding = st2.isMutableRootBinding;
    resolveReceiverRootExpr =
        [&](const Expr &expr,
            const ExprSubstitutions &substitutions,
            std::string &rootOut) -> bool {
          if (expr.kind == Expr::Kind::Name) {
            size_t matchedIndex = 0;
            if (const Expr *substitutedExpr =
                    findSubstitutedExpr(substitutions, expr.name, &matchedIndex)) {
              const ExprSubstitutions reducedSubstitutions =
                  removeSubstitutionAt(substitutions, matchedIndex);
              return resolveReceiverRootExpr(*substitutedExpr,
                                            reducedSubstitutions,
                                            rootOut);
            }
            return resolveBorrowRoot(expr.name, rootOut);
          }
          if (expr.kind != Expr::Kind::Call) {
            return false;
          }
          std::string builtinName;
          if (getBuiltinPointerName(expr, builtinName) && expr.args.size() == 1) {
            if (builtinName == "location" || builtinName == "dereference") {
              return resolveReceiverRootExpr(expr.args.front(),
                                            substitutions,
                                            rootOut);
            }
          }
          ExprSubstitutions nestedSubstitutions;
          const Expr *returnedValueExpr = nullptr;
          if (!appendCallSubstitutions(expr, substitutions, nestedSubstitutions,
                                       returnedValueExpr)) {
            return false;
          }
          return resolveReceiverRootExpr(*returnedValueExpr,
                                        nestedSubstitutions,
                                        rootOut);
        };
    resolveStandaloneRefRootExpr =
        [&](const Expr &expr,
            const ExprSubstitutions &substitutions,
            std::string &rootOut) -> bool {
          if (expr.kind == Expr::Kind::Name) {
            size_t matchedIndex = 0;
            if (const Expr *substitutedExpr =
                    findSubstitutedExpr(substitutions, expr.name, &matchedIndex)) {
              const ExprSubstitutions reducedSubstitutions =
                  removeSubstitutionAt(substitutions, matchedIndex);
              return resolveStandaloneRefRootExpr(*substitutedExpr,
                                                 reducedSubstitutions,
                                                 rootOut);
            }
            return false;
          }
          if (expr.kind == Expr::Kind::Call && expr.args.size() == 2) {
            std::string resolvedPath =
                preferredCollectionHelperResolvedPath(expr);
            if (resolvedPath.empty()) {
              resolvedPath = resolveCalleePath(expr);
            }
            (void)resolveConcreteCallPath(expr, resolvedPath);
            const std::string resolvedPathCanonical =
                canonicalizeLegacySoaRefHelperPath(resolvedPath);
            const bool resolvedCanonicalRefLike =
                isCanonicalSoaRefLikeHelperPath(resolvedPathCanonical);
            const bool resolvedExperimentalRefLike =
                isExperimentalSoaRefLikeHelperPath(resolvedPathCanonical);
            const bool isMethodRefCall =
                expr.isMethodCall &&
                (expr.name == "ref" ||
                 resolvedCanonicalRefLike || resolvedExperimentalRefLike);
            const bool isHelperRefCall =
                !expr.isMethodCall &&
                (isSimpleCallName(expr, "ref") ||
                 resolvedCanonicalRefLike || resolvedExperimentalRefLike);
            if ((isMethodRefCall || isHelperRefCall) &&
                resolveReceiverRootExpr(expr.args.front(),
                                       substitutions,
                                       rootOut)) {
              return !rootOut.empty();
            }
          }
          if (expr.kind != Expr::Kind::Call) {
            return false;
          }
          ExprSubstitutions nestedSubstitutions;
          const Expr *returnedValueExpr = nullptr;
          if (!appendCallSubstitutions(expr, substitutions, nestedSubstitutions,
                                       returnedValueExpr)) {
            return false;
          }
          return resolveStandaloneRefRootExpr(*returnedValueExpr,
                                             nestedSubstitutions,
                                             rootOut);
        };
  return PhaseStatus::Continue;
}

PhaseStatus SemanticsValidator::validateBindingReferencePhase5([[maybe_unused]] const std::vector<ParameterInfo> &params, [[maybe_unused]] std::unordered_map<std::string, BindingInfo> &locals, [[maybe_unused]] const Expr &stmt, [[maybe_unused]] const std::string &namespacePrefix, [[maybe_unused]] bool &handled, [[maybe_unused]] ValidateBindingState &st, ValidateBindingReferenceState &st2) {
  [[maybe_unused]] auto &allowBindings = st.allowBindings;
  [[maybe_unused]] auto &allowCompileTimeTypeBindings = st.allowCompileTimeTypeBindings;
  [[maybe_unused]] auto &failBindingDiagnostic = st.failBindingDiagnostic;
  [[maybe_unused]] auto &definitionTemplateArgs = st.definitionTemplateArgs;
  [[maybe_unused]] auto &info = st.info;
  [[maybe_unused]] const Expr &initializer = *st.initializer;
  [[maybe_unused]] auto &resolveNamedBinding = st.resolveNamedBinding;
  [[maybe_unused]] auto &resolvePointerRoot = st.resolvePointerRoot;
  [[maybe_unused]] const Expr &init = *st2.init;
  [[maybe_unused]] auto &resolveDirectBorrowStorageTargetType = st2.resolveDirectBorrowStorageTargetType;
  [[maybe_unused]] auto &resolvePointerTargetType = st2.resolvePointerTargetType;
  [[maybe_unused]] auto &resolveBorrowRoot = st2.resolveBorrowRoot;
  [[maybe_unused]] auto &resolveStandaloneRefRootExpr = st2.resolveStandaloneRefRootExpr;
  [[maybe_unused]] auto &isStandaloneRefCall = st2.isStandaloneRefCall;
  [[maybe_unused]] auto &hasBorrowConflictForRoot = st2.hasBorrowConflictForRoot;
  [[maybe_unused]] auto &isMutableRootBinding = st2.isMutableRootBinding;
	    std::string pointerName;
	    const bool initIsLocation =
	        init.kind == Expr::Kind::Call && getBuiltinPointerName(init, pointerName) && pointerName == "location" &&
	        init.args.size() == 1;
	    std::string safeTargetType;
	    const bool initIsDirectBorrowStorage = resolveDirectBorrowStorageTargetType(init, safeTargetType);
	    const bool initIsPointerLike = resolvePointerTargetType(init, safeTargetType);
	    const bool initIsBorrowedFieldAccess =
	        init.kind == Expr::Kind::Call && init.isFieldAccess && init.args.size() == 1 &&
	        initIsPointerLike;
	    if (!initIsLocation && !initIsDirectBorrowStorage && !initIsPointerLike &&
	        !currentValidationState_.context.definitionIsUnsafe) {
	      return st2.done(st.done(failBindingDiagnostic("Reference bindings require location(...)")));
	    }
	    if (initIsLocation || initIsDirectBorrowStorage ||
	        (!currentValidationState_.context.definitionIsUnsafe && initIsPointerLike)) {
      if (!errorTypesMatch(safeTargetType, info.typeTemplateArg, namespacePrefix)) {
        return st2.done(st.done(failBindingDiagnostic("Reference binding type mismatch")));
      }
    }
	    if (!initIsLocation && !initIsDirectBorrowStorage &&
	        !initIsBorrowedFieldAccess &&
	        !currentValidationState_.context.definitionIsUnsafe) {
	      std::string borrowRoot;
	      const ExprSubstitutions substitutions;
	      const bool resolvedStandaloneRoot =
	          resolveStandaloneRefRootExpr(init, substitutions, borrowRoot);
      if (isStandaloneRefCall(init) &&
          (!resolvedStandaloneRoot || borrowRoot.empty())) {
        return st2.done(st.done(failBindingDiagnostic("Reference binding requires borrow root")));
      }
      if (resolvedStandaloneRoot && !borrowRoot.empty()) {
        if (hasBorrowConflictForRoot(borrowRoot, info.isMutable)) {
          return st2.done(st.done(failBindingDiagnostic(
              "borrow conflict: " + borrowRoot + " (root: " + borrowRoot +
              ", sink: " + stmt.name + ")")));
        }
        if (info.isMutable && !isMutableRootBinding(borrowRoot)) {
          return st2.done(st.done(failBindingDiagnostic("Reference binding requires mutable root: " +
                                       borrowRoot)));
        }
        info.referenceRoot = std::move(borrowRoot);
      }
      if (!validateBuiltinComparableKeyType(info, definitionTemplateArgs, error_)) {
        return st2.done(st.done(false));
      }
      insertLocalBinding(locals, stmt.name, std::move(info));
      return st2.done(st.done(true));
    }
    if (!initIsLocation && currentValidationState_.context.definitionIsUnsafe) {
      std::string pointerTargetType;
      if (!resolvePointerTargetType(init, pointerTargetType)) {
        return st2.done(st.done(failBindingDiagnostic("unsafe Reference bindings require pointer-like initializer")));
      }
      if (!errorTypesMatch(pointerTargetType, info.typeTemplateArg, namespacePrefix)) {
        return st2.done(st.done(failBindingDiagnostic("unsafe Reference binding type mismatch")));
      }
      std::string borrowRoot;
      if (resolvePointerRoot(init, borrowRoot)) {
        info.referenceRoot = std::move(borrowRoot);
      }
      info.isUnsafeReference = true;
      if (!validateBuiltinComparableKeyType(info, definitionTemplateArgs, error_)) {
        return st2.done(st.done(false));
      }
      insertLocalBinding(locals, stmt.name, std::move(info));
      return st2.done(st.done(true));
    }
  st2.target = &((initIsLocation || initIsDirectBorrowStorage)
	                             ? init.args.front()
	                             : init);
  [[maybe_unused]] const Expr &target = *st2.target;
  [[maybe_unused]] auto &resolveBorrowRootExpr = st2.resolveBorrowRootExpr;
    resolveBorrowRootExpr = [&](const Expr &targetExpr, std::string &rootOut) -> bool {
      if (targetExpr.kind == Expr::Kind::Name) {
        return resolveBorrowRoot(targetExpr.name, rootOut);
      }
      std::string builtinName;
      if (targetExpr.kind == Expr::Kind::Call && getBuiltinPointerName(targetExpr, builtinName) &&
          builtinName == "dereference" && targetExpr.args.size() == 1) {
        return resolvePointerRoot(targetExpr.args.front(), rootOut);
      }
      if (targetExpr.kind == Expr::Kind::Call && targetExpr.isFieldAccess && targetExpr.args.size() == 1) {
        std::string receiverRoot;
        if (!resolveBorrowRootExpr(targetExpr.args.front(), receiverRoot) || receiverRoot.empty()) {
          return false;
        }
        rootOut = receiverRoot + "." + targetExpr.name;
        return true;
      }
      return false;
    };
  return PhaseStatus::Continue;
}

PhaseStatus SemanticsValidator::validateBindingReferencePhase6([[maybe_unused]] const std::vector<ParameterInfo> &params, [[maybe_unused]] std::unordered_map<std::string, BindingInfo> &locals, [[maybe_unused]] const Expr &stmt, [[maybe_unused]] const std::string &namespacePrefix, [[maybe_unused]] bool &handled, [[maybe_unused]] ValidateBindingState &st, ValidateBindingReferenceState &st2) {
  [[maybe_unused]] auto &allowBindings = st.allowBindings;
  [[maybe_unused]] auto &allowCompileTimeTypeBindings = st.allowCompileTimeTypeBindings;
  [[maybe_unused]] auto &failBindingDiagnostic = st.failBindingDiagnostic;
  [[maybe_unused]] auto &definitionTemplateArgs = st.definitionTemplateArgs;
  [[maybe_unused]] auto &info = st.info;
  [[maybe_unused]] const Expr &initializer = *st.initializer;
  [[maybe_unused]] auto &resolveNamedBinding = st.resolveNamedBinding;
  [[maybe_unused]] auto &resolvePointerRoot = st.resolvePointerRoot;
  [[maybe_unused]] const Expr &target = *st2.target;
  [[maybe_unused]] auto &resolveBorrowRootExpr = st2.resolveBorrowRootExpr;
    std::string borrowRoot;
    if (!resolveBorrowRootExpr(target, borrowRoot) || borrowRoot.empty()) {
      return st2.done(st.done(failBindingDiagnostic("Reference bindings require location(...)")));
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
    const bool conflict = info.isMutable ? (sawMutableBorrow || sawImmutableBorrow) : sawMutableBorrow;
    if (conflict && !currentValidationState_.context.definitionIsUnsafe) {
      return st2.done(st.done(failBindingDiagnostic("borrow conflict: " + borrowRoot + " (root: " + borrowRoot +
                                   ", sink: " + stmt.name + ")")));
    }
    info.referenceRoot = std::move(borrowRoot);
    info.isUnsafeReference = currentValidationState_.context.definitionIsUnsafe;
  return PhaseStatus::Continue;
}

} // namespace primec::semantics
