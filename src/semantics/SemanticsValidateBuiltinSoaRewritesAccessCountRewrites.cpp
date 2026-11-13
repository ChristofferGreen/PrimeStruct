#include "SemanticsValidateBuiltinSoaRewrites.h"

#include "SemanticsHelpers.h"
#include "SemanticsValidateBuiltinSoaMetadata.h"
#include "SemanticsValidateSoaRewriteSupport.h"
#include "SemanticsValidateSoaBindingExtraction.h"
#include "primec/support/CollectionHelperNames.h"

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace primec {

struct BuiltinSoaAccessRewriteFlags {
  bool getHelper = false;
  bool getRefHelper = false;
  bool refHelper = false;
  bool refRefHelper = false;
  std::unordered_set<std::string> visiblePublicSoaHelpers;
};

void rewriteBuiltinSoaAccessExpr(Expr &expr,
                                 const SoaBindingMap &bindings,
                                 const BuiltinSoaRewriteScope &scope,
                                 const BuiltinSoaAccessRewriteFlags &flags) {
  const bool preserveGetHelper = flags.getHelper;
  const bool preserveGetRefHelper = flags.getRefHelper;
  const bool preserveRefHelper = flags.refHelper;
  const bool preserveRefRefHelper = flags.refRefHelper;
  const std::unordered_set<std::string> &visiblePublicSoaHelpers = flags.visiblePublicSoaHelpers;
  auto findBuiltinVectorValueBinding = [&](const Expr &candidate) {
    return primec::findBuiltinVectorValueBinding(candidate, bindings, scope);
  };
  auto findBuiltinSoaValueBinding = [&](const Expr &candidate) {
    return primec::findBuiltinSoaReceiverValueBinding(candidate, bindings, scope);
  };

  for (Expr &arg : expr.args) {
    rewriteBuiltinSoaAccessExpr(arg, bindings, scope, flags);
  }
  if (expr.kind != Expr::Kind::Call || expr.args.size() != 2 ||
      !expr.templateArgs.empty() ||
      semantics::hasNamedArguments(expr.argNames) ||
      expr.hasBodyArguments ||
      !expr.bodyArguments.empty()) {
    return;
  }
  const std::string helperName = builtinSoaAccessHelperName(expr.name);
  if (helperName.empty()) {
    return;
  }
  const auto receiverBinding = findBuiltinSoaValueBinding(expr.args.front());
  const std::string resolvedHelperName =
      receiverBinding.has_value() && receiverBinding->borrowed
          ? borrowedBuiltinSoaAccessHelperName(helperName)
          : helperName;
  if (resolvedHelperName.empty()) {
    return;
  }
  if (helperName == resolvedHelperName &&
      ((resolvedHelperName == "get" && preserveGetHelper) ||
       (resolvedHelperName == collection_helpers::kGetRef && preserveGetRefHelper) ||
       (resolvedHelperName == "ref" && preserveRefHelper) ||
       (resolvedHelperName == collection_helpers::kRefRef && preserveRefRefHelper))) {
    return;
  }
  const bool hasBuiltinSoaReceiver = receiverBinding.has_value();
  const bool hasBuiltinVectorReceiver =
      !receiverBinding.has_value() &&
      findBuiltinVectorValueBinding(expr.args.front()).has_value();
  if (!hasBuiltinSoaReceiver && !hasBuiltinVectorReceiver) {
    return;
  }

  const auto fallbackVectorBinding = receiverBinding.has_value()
                                         ? std::optional<semantics::BindingInfo>{}
                                         : findBuiltinVectorValueBinding(expr.args.front());

  expr.isMethodCall = false;
  expr.isFieldAccess = false;
  expr.name = visiblePublicSoaHelpers.count(resolvedHelperName) > 0
                  ? semantics::publicSoaHelperTargetPath(resolvedHelperName)
                  : semantics::compatibilitySoaHelperTargetPath(resolvedHelperName);
  expr.namespacePrefix.clear();
  expr.templateArgs.clear();
  if (receiverBinding.has_value() && !receiverBinding->binding.typeTemplateArg.empty()) {
    expr.templateArgs.push_back(receiverBinding->binding.typeTemplateArg);
  } else if (fallbackVectorBinding.has_value() && !fallbackVectorBinding->typeTemplateArg.empty()) {
    expr.templateArgs.push_back(fallbackVectorBinding->typeTemplateArg);
  }
}

bool rewriteBuiltinSoaAccessCalls(Program &program, std::string &error) {
  error.clear();
  BuiltinSoaAccessRewriteFlags flags;
  flags.getHelper = hasVisibleRootSoaHelper(program, "get");
  flags.getRefHelper = hasVisibleRootSoaHelper(program, collection_helpers::kGetRef);
  flags.refHelper = hasVisibleRootSoaHelper(program, "ref");
  flags.refRefHelper = hasVisibleRootSoaHelper(program, collection_helpers::kRefRef);
  for (std::string_view helperName : {collection_helpers::kGetRef, collection_helpers::kRefRef}) {
    if (hasVisiblePublicSoaHelperDefinition(program, helperName)) {
      flags.visiblePublicSoaHelpers.insert(std::string(helperName));
    }
  }
  rewriteBuiltinSoaDefinitions(
      program,
      extractBuiltinSoaVectorOrBorrowedReturnBinding,
      [&](Expr &expr, const SoaBindingMap &bindings, const BuiltinSoaRewriteScope &scope) {
        rewriteBuiltinSoaAccessExpr(expr, bindings, scope, flags);
      });
  return true;
}

struct BuiltinSoaCountRewriteFlags {
  bool countHelper = false;
  bool countRefHelper = false;
  std::unordered_set<std::string> visiblePublicSoaHelpers;
};

void rewriteBuiltinSoaCountExpr(Expr &expr,
                                const SoaBindingMap &bindings,
                                const BuiltinSoaRewriteScope &scope,
                                const BuiltinSoaCountRewriteFlags &flags) {
  const bool preserveCountHelper = flags.countHelper;
  const bool preserveCountRefHelper = flags.countRefHelper;
  const std::unordered_set<std::string> &visiblePublicSoaHelpers = flags.visiblePublicSoaHelpers;
  auto findBuiltinVectorValueBinding = [&](const Expr &candidate) {
    return primec::findBuiltinVectorValueBinding(candidate, bindings, scope);
  };
  auto findBuiltinSoaValueBinding = [&](const Expr &candidate) {
    return primec::findBuiltinSoaReceiverValueBinding(candidate, bindings, scope);
  };

  for (Expr &arg : expr.args) {
    rewriteBuiltinSoaCountExpr(arg, bindings, scope, flags);
  }
  if (expr.kind != Expr::Kind::Call || expr.args.size() != 1 ||
      !expr.templateArgs.empty() ||
      semantics::hasNamedArguments(expr.argNames) ||
      expr.hasBodyArguments ||
      !expr.bodyArguments.empty()) {
    return;
  }
  const std::string helperName = builtinSoaCountHelperName(expr.name);
  if (helperName.empty()) {
    return;
  }
  const auto receiverBinding = findBuiltinSoaValueBinding(expr.args.front());
  const std::string resolvedHelperName =
      receiverBinding.has_value() && receiverBinding->borrowed
          ? borrowedBuiltinSoaCountHelperName(helperName)
          : helperName;
  if (resolvedHelperName.empty()) {
    return;
  }
  if (helperName == resolvedHelperName &&
      ((resolvedHelperName == "count" && preserveCountHelper) ||
       (resolvedHelperName == collection_helpers::kCountRef && preserveCountRefHelper))) {
    return;
  }
  const bool explicitOldSoaCount = isOldExplicitSoaCountHelperName(expr.name);
  const auto fallbackVectorBinding =
      receiverBinding.has_value() || !explicitOldSoaCount
          ? std::optional<semantics::BindingInfo>{}
          : findBuiltinVectorValueBinding(expr.args.front());
  if (!receiverBinding.has_value() && !fallbackVectorBinding.has_value()) {
    return;
  }

  expr.isMethodCall = false;
  expr.isFieldAccess = false;
  expr.name = visiblePublicSoaHelpers.count(resolvedHelperName) > 0
                  ? semantics::publicSoaHelperTargetPath(resolvedHelperName)
                  : semantics::compatibilitySoaHelperTargetPath(resolvedHelperName);
  expr.namespacePrefix.clear();
  expr.templateArgs.clear();
  if (receiverBinding.has_value() && !receiverBinding->binding.typeTemplateArg.empty()) {
    expr.templateArgs.push_back(receiverBinding->binding.typeTemplateArg);
  } else if (fallbackVectorBinding.has_value() && !fallbackVectorBinding->typeTemplateArg.empty()) {
    expr.templateArgs.push_back(fallbackVectorBinding->typeTemplateArg);
  }
}

bool rewriteBuiltinSoaCountCalls(Program &program, std::string &error) {
  error.clear();
  BuiltinSoaCountRewriteFlags flags;
  flags.countHelper = hasVisibleRootSoaHelper(program, "count");
  flags.countRefHelper = hasVisibleRootSoaHelper(program, collection_helpers::kCountRef);
  if (hasVisiblePublicSoaHelperDefinition(program, collection_helpers::kCountRef)) {
    flags.visiblePublicSoaHelpers.insert(collection_helpers::kCountRef);
  }
  rewriteBuiltinSoaDefinitions(
      program,
      extractBuiltinSoaVectorOrBorrowedReturnBinding,
      [&](Expr &expr, const SoaBindingMap &bindings, const BuiltinSoaRewriteScope &scope) {
        rewriteBuiltinSoaCountExpr(expr, bindings, scope, flags);
      });
  return true;
}

} // namespace primec
