#include "SemanticsValidator.h"

#include <algorithm>
#include <string>
#include <vector>

namespace primec::semantics {

bool SemanticsValidator::validateExprBodyArguments(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &expr,
    std::string &resolved,
    bool &resolvedMethod,
    const BuiltinCollectionDispatchResolverAdapters &builtinCollectionDispatchResolverAdapters,
    const std::vector<Expr> *enclosingStatements,
    size_t statementIndex) {
  auto failExprBodyArgumentDiagnostic = [&](const Expr &diagnosticExpr,
                                            std::string message) -> bool {
    return failExprDiagnostic(diagnosticExpr, std::move(message));
  };
  if (!(expr.hasBodyArguments || !expr.bodyArguments.empty()) ||
      isBuiltinBlockCall(expr)) {
    return true;
  }

  std::string remappedRemovedMapBodyArgumentTarget;
  const bool remappedRemovedMapTarget =
      this->resolveRemovedMapBodyArgumentTarget(
          expr, resolved, params, locals,
          builtinCollectionDispatchResolverAdapters,
          remappedRemovedMapBodyArgumentTarget);
  if (remappedRemovedMapTarget) {
    resolved = remappedRemovedMapBodyArgumentTarget;
    resolvedMethod = false;
  } else if (!resolvedMethod &&
             !this->shouldPreserveRemovedCollectionHelperPath(resolved)) {
    resolved = preferVectorStdlibHelperPath(resolved);
  }

  auto bindingTypeText = [](const BindingInfo &binding) {
    if (binding.typeTemplateArg.empty()) {
      return binding.typeName;
    }
    return binding.typeName + "<" + binding.typeTemplateArg + ">";
  };
  auto inferPointerLikeTargetTypeText = [&](const Expr &receiverExpr) -> std::string {
    if (receiverExpr.kind == Expr::Kind::Name) {
      if (const BindingInfo *binding = findBinding(params, locals, receiverExpr.name);
          binding != nullptr &&
          (binding->typeName == "Pointer" || binding->typeName == "Reference") &&
          !binding->typeTemplateArg.empty()) {
        return binding->typeTemplateArg;
      }
      return "";
    }

    if (receiverExpr.kind != Expr::Kind::Call) {
      return "";
    }

    std::string builtinName;
    if (getBuiltinPointerName(receiverExpr, builtinName) && builtinName == "location" &&
        receiverExpr.args.size() == 1) {
      const Expr &target = receiverExpr.args.front();
      if (target.kind == Expr::Kind::Name) {
        if (const BindingInfo *binding = findBinding(params, locals, target.name)) {
          return bindingTypeText(*binding);
        }
      }
      const ReturnKind targetKind = inferExprReturnKind(target, params, locals);
      return typeNameForReturnKind(targetKind);
    }

    const std::string receiverPath = resolveCalleePath(receiverExpr);
    auto defIt = defMap_.find(receiverPath);
    if (defIt == defMap_.end() || defIt->second == nullptr) {
      return "";
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
      base = normalizeBindingTypeName(base);
      if ((base == "Pointer" || base == "Reference") && !arg.empty()) {
        return arg;
      }
    }
    return "";
  };
  if (expr.isMethodCall && !expr.args.empty()) {
    std::string namespacedCollection;
    std::string namespacedHelper;
    if (getNamespacedCollectionHelperName(expr, namespacedCollection, namespacedHelper) &&
        namespacedCollection == "vector" &&
        (namespacedHelper == "count" || namespacedHelper == "capacity")) {
      const std::string targetTypeText =
          inferPointerLikeTargetTypeText(expr.args.front());
      if (!targetTypeText.empty()) {
        resolved = "/" + normalizeBindingTypeName(targetTypeText) + "/" + namespacedHelper;
        resolvedMethod = false;
      }
    }
  }

  if (resolvedMethod ||
      (!hasDeclaredDefinitionPath(resolved) &&
       !hasImportedDefinitionPath(resolved))) {
    return failExprBodyArgumentDiagnostic(
        expr, "block arguments require a definition target: " + resolved);
  }

  std::unordered_map<std::string, BindingInfo> blockLocals = locals;
  const std::vector<Expr> *postMergeStatements = enclosingStatements;
  const size_t postMergeStartIndex = enclosingStatements == nullptr
                                         ? 0
                                         : std::min(statementIndex + 1, enclosingStatements->size());
  std::vector<BorrowLivenessRange> livenessRanges;
  livenessRanges.reserve(2);
  OnErrorScope onErrorScope(*this, std::nullopt);
  BorrowEndScope borrowScope(*this,
                             currentValidationState_.endedReferenceBorrows);
  for (size_t bodyIndex = 0; bodyIndex < expr.bodyArguments.size();
       ++bodyIndex) {
    const Expr &bodyExpr = expr.bodyArguments[bodyIndex];
    if (!validateStatement(params, blockLocals, bodyExpr, ReturnKind::Unknown,
                           false, true, nullptr, expr.namespacePrefix,
                           &expr.bodyArguments, bodyIndex)) {
      return false;
    }
    livenessRanges.clear();
    livenessRanges.push_back(BorrowLivenessRange{&expr.bodyArguments, bodyIndex + 1});
    if (postMergeStatements != nullptr && postMergeStartIndex < postMergeStatements->size()) {
      livenessRanges.push_back(BorrowLivenessRange{postMergeStatements, postMergeStartIndex});
    }
    expireReferenceBorrowsForRanges(params, blockLocals, livenessRanges);
  }
  return true;
}

} // namespace primec::semantics
