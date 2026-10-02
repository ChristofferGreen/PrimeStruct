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

namespace primec::semantics {
using namespace statementBindingsHelpers;

PhaseStatus SemanticsValidator::validateBindingPhase6([[maybe_unused]] const std::vector<ParameterInfo> &params, [[maybe_unused]] std::unordered_map<std::string, BindingInfo> &locals, [[maybe_unused]] const Expr &stmt, [[maybe_unused]] const std::string &namespacePrefix, [[maybe_unused]] bool &handled, ValidateBindingState &st) {
  [[maybe_unused]] auto &allowBindings = st.allowBindings;
  [[maybe_unused]] auto &allowCompileTimeTypeBindings = st.allowCompileTimeTypeBindings;
  [[maybe_unused]] auto &failBindingDiagnostic = st.failBindingDiagnostic;
  [[maybe_unused]] auto &info = st.info;
  [[maybe_unused]] auto &hasExplicitType = st.hasExplicitType;
  [[maybe_unused]] auto &explicitAutoType = st.explicitAutoType;
  [[maybe_unused]] const Expr &initializer = *st.initializer;
  [[maybe_unused]] auto &isStandaloneSoaFieldViewInitializer = st.isStandaloneSoaFieldViewInitializer;
  BindingInfo fieldViewBinding = info;
  if ((!hasExplicitType || explicitAutoType) && info.typeName != "Reference") {
    BindingInfo inferredBinding;
    if (inferBindingTypeFromInitializer(initializer, params, locals, inferredBinding, &stmt)) {
      fieldViewBinding = std::move(inferredBinding);
    }
  }
  if (isSoaFieldViewBindingType(fieldViewBinding) ||
      isStandaloneSoaFieldViewInitializer()) {
    auto hasBorrowConflictForRoot =
        [&](const std::string &borrowRoot, bool requestMutable) -> bool {
          if (borrowRoot.empty() ||
              currentValidationState_.context.definitionIsUnsafe) {
            return false;
          }
          bool sawMutableBorrow = false;
          bool sawImmutableBorrow = false;
          auto observeBorrow = [&](const std::string &bindingName,
                                   const BindingInfo &binding) {
            if (currentValidationState_.endedReferenceBorrows.count(bindingName) > 0) {
              return;
            }
            const std::string root =
                referenceRootForBorrowBinding(bindingName, binding);
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
          return requestMutable ? (sawMutableBorrow || sawImmutableBorrow)
                                : sawMutableBorrow;
        };
    auto isMutableRootBinding = [&](const std::string &borrowRoot) -> bool {
      if (borrowRoot.empty()) {
        return false;
      }
      if (const BindingInfo *paramBinding = findParamBinding(params, borrowRoot)) {
        return paramBinding->isMutable;
      }
      auto it = locals.find(borrowRoot);
      return it != locals.end() && it->second.isMutable;
    };
    auto resolveBorrowRootName = [&](const std::string &name,
                                     std::string &rootOut) -> bool {
      if (const BindingInfo *paramBinding = findParamBinding(params, name)) {
        if (isBorrowTrackedBindingType(*paramBinding)) {
          rootOut = referenceRootForBorrowBinding(name, *paramBinding);
        } else {
          rootOut = name;
        }
        return true;
      }
      auto it = locals.find(name);
      if (it == locals.end()) {
        return false;
      }
      if (isBorrowTrackedBindingType(it->second)) {
        rootOut = referenceRootForBorrowBinding(it->first, it->second);
      } else {
        rootOut = name;
      }
      return true;
    };
    using ExprSubstitutions = std::vector<std::pair<std::string, const Expr *>>;
    auto findSubstitutedExpr = [&](const ExprSubstitutions &substitutions,
                                   const std::string &name,
                                   size_t *matchedIndexOut = nullptr) -> const Expr * {
      for (size_t index = substitutions.size(); index > 0; --index) {
        if (substitutions[index - 1].first == name) {
          if (matchedIndexOut != nullptr) {
            *matchedIndexOut = index - 1;
          }
          return substitutions[index - 1].second;
        }
      }
      return nullptr;
    };
    auto removeSubstitutionAt = [&](const ExprSubstitutions &substitutions,
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
    };
    auto appendCallSubstitutions =
        [&](const Expr &callExpr,
            const ExprSubstitutions &baseSubstitutions,
            ExprSubstitutions &extendedSubstitutions,
            const Expr *&returnedValueExprOut) -> bool {
          returnedValueExprOut = nullptr;
          std::string resolvedCallPath =
              preferredCollectionHelperResolvedPath(callExpr);
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
                  resolveExprConcreteCallPath(
                      params, locals, callExpr, resolvedCallPath);
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
          if (!buildOrderedArguments(nestedParams, callExpr.args, callExpr.argNames,
                                     nestedOrderedArgs, nestedArgError)) {
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
        };
    std::function<bool(const Expr &, const ExprSubstitutions &, std::string &)>
        resolveReceiverRootExpr;
    std::function<bool(const Expr &, const ExprSubstitutions &, std::string &)>
        resolveStandaloneFieldViewRootExpr;
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
            return resolveBorrowRootName(expr.name, rootOut);
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
    resolveStandaloneFieldViewRootExpr =
        [&](const Expr &expr,
            const ExprSubstitutions &substitutions,
            std::string &rootOut) -> bool {
          if (expr.kind == Expr::Kind::Name) {
            size_t matchedIndex = 0;
            if (const Expr *substitutedExpr =
                    findSubstitutedExpr(substitutions, expr.name, &matchedIndex)) {
              const ExprSubstitutions reducedSubstitutions =
                  removeSubstitutionAt(substitutions, matchedIndex);
              return resolveStandaloneFieldViewRootExpr(*substitutedExpr,
                                                       reducedSubstitutions,
                                                       rootOut);
            }
            const BindingInfo *binding = findParamBinding(params, expr.name);
            if (binding == nullptr) {
              auto localIt = locals.find(expr.name);
              if (localIt != locals.end()) {
                binding = &localIt->second;
              }
            }
            if (binding != nullptr && isBorrowTrackedBindingType(*binding)) {
              rootOut = referenceRootForBorrowBinding(expr.name, *binding);
              return !rootOut.empty();
            }
            return false;
          }
          if (expr.kind == Expr::Kind::Call && expr.args.size() >= 1) {
            std::string resolvedFieldViewPath =
                preferredCollectionHelperResolvedPath(expr);
            if (resolvedFieldViewPath.empty()) {
              resolvedFieldViewPath = resolveCalleePath(expr);
            }
            if (isBuiltinSoaFieldViewExpr(expr, params, locals, nullptr) ||
                isExperimentalSoaFieldViewHelperPath(resolvedFieldViewPath)) {
              const Expr *receiverExpr = &expr.args.front();
              if (resolveReceiverRootExpr(*receiverExpr, substitutions, rootOut)) {
                return !rootOut.empty();
              }
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
          return resolveStandaloneFieldViewRootExpr(*returnedValueExpr,
                                                    nestedSubstitutions,
                                                    rootOut);
        };

    std::string borrowRoot;
    const ExprSubstitutions substitutions;
    if (!resolveStandaloneFieldViewRootExpr(initializer, substitutions, borrowRoot) ||
        borrowRoot.empty()) {
      return st.done(failBindingDiagnostic("field-view binding requires borrow root"));
    }
    if (hasBorrowConflictForRoot(borrowRoot, info.isMutable)) {
      return st.done(failBindingDiagnostic("borrow conflict: " + borrowRoot + " (root: " +
                                   borrowRoot + ", sink: " + stmt.name + ")"));
    }
    if (info.isMutable && !isMutableRootBinding(borrowRoot)) {
      return st.done(failBindingDiagnostic("field-view binding requires mutable root: " +
                                   borrowRoot));
    }
    info.referenceRoot = std::move(borrowRoot);
  }
  return PhaseStatus::Continue;
}

PhaseStatus SemanticsValidator::validateBindingPhase7([[maybe_unused]] const std::vector<ParameterInfo> &params, [[maybe_unused]] std::unordered_map<std::string, BindingInfo> &locals, [[maybe_unused]] const Expr &stmt, [[maybe_unused]] const std::string &namespacePrefix, [[maybe_unused]] bool &handled, ValidateBindingState &st) {
  [[maybe_unused]] auto &allowBindings = st.allowBindings;
  [[maybe_unused]] auto &allowCompileTimeTypeBindings = st.allowCompileTimeTypeBindings;
  [[maybe_unused]] auto &definitionTemplateArgs = st.definitionTemplateArgs;
  [[maybe_unused]] auto &info = st.info;
  if (!validateBuiltinComparableKeyType(info, definitionTemplateArgs, error_)) {
    return st.done(false);
  }
  insertLocalBinding(locals, stmt.name, std::move(info));
  return st.done(true);
  return PhaseStatus::Continue;
}

} // namespace primec::semantics
