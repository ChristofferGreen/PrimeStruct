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

struct BuiltinSoaMutatorPreserveFlags {
  bool pushHelper = false;
  bool reserveHelper = false;
  bool vectorPushHelper = false;
  bool vectorReserveHelper = false;
};

void rewriteBuiltinSoaMutatorExpr(Expr &expr,
                                  const SoaBindingMap &bindings,
                                  const BuiltinSoaRewriteScope &scope,
                                  const BuiltinSoaMutatorPreserveFlags &flags) {
  const SoaBindingMap &soaCollectionReturnDefinitions = scope.soaCollectionReturnDefinitions;
  const std::string &definitionNamespace = scope.definitionNamespace;
  const bool preservePushHelper = flags.pushHelper;
  const bool preserveReserveHelper = flags.reserveHelper;
  const bool preserveVectorPushHelper = flags.vectorPushHelper;
  const bool preserveVectorReserveHelper = flags.vectorReserveHelper;
  auto findBuiltinVectorValueBinding = [&](const Expr &candidate) {
    return primec::findBuiltinVectorValueBinding(candidate, bindings, scope);
  };
  auto findBuiltinSoaValueBinding = [&](const Expr &candidate) -> std::optional<semantics::BindingInfo> {
    if (candidate.kind == Expr::Kind::Name) {
      auto bindingIt = bindings.find(candidate.name);
      if (bindingIt != bindings.end() && isBuiltinSoaVectorBinding(bindingIt->second)) {
        return bindingIt->second;
      }
      return std::nullopt;
    }
    if (candidate.kind != Expr::Kind::Call || candidate.isBinding) {
      return std::nullopt;
    }
    std::string collectionName;
    if (semantics::getBuiltinCollectionName(candidate, collectionName) &&
        collectionName == semantics::internalSoaCollectionTypeName() &&
        candidate.templateArgs.size() == 1) {
      semantics::BindingInfo binding;
      binding.typeName = semantics::internalSoaCollectionTypeName();
      binding.typeTemplateArg = candidate.templateArgs.front();
      return binding;
    }
    for (const std::string &candidatePath : candidateDefinitionPaths(candidate, definitionNamespace)) {
      auto returnIt = soaCollectionReturnDefinitions.find(candidatePath);
      if (returnIt != soaCollectionReturnDefinitions.end() && isBuiltinSoaVectorBinding(returnIt->second)) {
        return returnIt->second;
      }
    }
    if (!candidate.isMethodCall &&
        semantics::isSimpleCallName(candidate, "dereference") &&
        candidate.args.size() == 1) {
      const Expr &derefTarget = candidate.args.front();
      if (derefTarget.kind == Expr::Kind::Name) {
        auto bindingIt = bindings.find(derefTarget.name);
        if (bindingIt != bindings.end()) {
          return extractBuiltinCollectionBindingFromWrappedTypeText(
              bindingTypeText(bindingIt->second),
              semantics::internalSoaCollectionTypeName());
        }
      }
      std::string accessName;
      if (semantics::getBuiltinArrayAccessName(derefTarget, accessName) &&
          derefTarget.args.size() == 2 &&
          derefTarget.args.front().kind == Expr::Kind::Name) {
        auto bindingIt = bindings.find(derefTarget.args.front().name);
        if (bindingIt != bindings.end()) {
          return extractBuiltinCollectionBindingFromWrappedTypeText(
              bindingTypeText(bindingIt->second),
              semantics::internalSoaCollectionTypeName());
        }
      }
    }
    return std::nullopt;
  };

  for (Expr &arg : expr.args) {
    rewriteBuiltinSoaMutatorExpr(arg, bindings, scope, flags);
  }
  if (expr.kind != Expr::Kind::Call || expr.args.size() != 2 ||
      !expr.templateArgs.empty() ||
      semantics::hasNamedArguments(expr.argNames) ||
      expr.hasBodyArguments ||
      !expr.bodyArguments.empty()) {
    return;
  }
  const std::string rawHelperName =
      expr.namespacePrefix.empty() ? expr.name : expr.namespacePrefix + "/" + expr.name;
  const std::string helperName = builtinSoaMutatorHelperName(rawHelperName);
  if (helperName.empty()) {
    return;
  }
  if ((helperName == "push" && preservePushHelper) ||
      (helperName == "reserve" && preserveReserveHelper)) {
    return;
  }
  const auto receiverBinding = findBuiltinSoaValueBinding(expr.args.front());
  const auto fallbackVectorBinding =
      receiverBinding.has_value()
          ? std::optional<semantics::BindingInfo>{}
          : findBuiltinVectorValueBinding(expr.args.front());
  const std::string explicitOldHelperName = oldExplicitSoaMutatorHelperName(rawHelperName);
  const bool preserveVectorHelper =
      (helperName == "push" && preserveVectorPushHelper) ||
      (helperName == "reserve" && preserveVectorReserveHelper);
  if (fallbackVectorBinding.has_value() && preserveVectorHelper) {
    if (expr.isMethodCall) {
      expr.isMethodCall = false;
      expr.isFieldAccess = false;
      expr.name = semantics::samePathSoaHelperTargetPath(helperName);
      expr.namespacePrefix.clear();
      expr.templateArgs.clear();
    }
    return;
  }
  if (fallbackVectorBinding.has_value() && explicitOldHelperName.empty()) {
    return;
  }
  if (!receiverBinding.has_value() && !fallbackVectorBinding.has_value()) {
    return;
  }

  expr.isMethodCall = false;
  expr.isFieldAccess = false;
  expr.name = semantics::compatibilitySoaHelperTargetPath(helperName);
  expr.namespacePrefix.clear();
  expr.templateArgs.clear();
  if (receiverBinding.has_value() && !receiverBinding->typeTemplateArg.empty()) {
    expr.templateArgs.push_back(receiverBinding->typeTemplateArg);
  } else if (fallbackVectorBinding.has_value() &&
             !fallbackVectorBinding->typeTemplateArg.empty()) {
    expr.templateArgs.push_back(fallbackVectorBinding->typeTemplateArg);
  }
}

bool rewriteBuiltinSoaMutatorCalls(Program &program, std::string &error) {
  error.clear();
  const BuiltinSoaMutatorPreserveFlags flags{
      hasVisibleRootSoaHelper(program, "push"),
      hasVisibleRootSoaHelper(program, "reserve"),
      hasVisibleRootSoaHelperForReceiverType(program, "push", "vector"),
      hasVisibleRootSoaHelperForReceiverType(program, "reserve", "vector")};
  rewriteBuiltinSoaDefinitions(
      program,
      extractBuiltinSoaVectorReturnBinding,
      [&](Expr &expr, const SoaBindingMap &bindings, const BuiltinSoaRewriteScope &scope) {
        rewriteBuiltinSoaMutatorExpr(expr, bindings, scope, flags);
      });
  return true;
}

} // namespace primec
