#include "SemanticsValidateBuiltinSoaRewrites.h"

#include "SemanticsHelpers.h"
#include "SemanticsValidateBuiltinSoaMetadata.h"
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

void rewriteBuiltinSoaMutatorExpr(
    Expr &expr,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &vectorReturnDefinitions,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaCollectionReturnDefinitions,
    const std::string &definitionNamespace,
    bool preservePushHelper,
    bool preserveReserveHelper,
    bool preserveVectorPushHelper,
    bool preserveVectorReserveHelper);

void rewriteBuiltinSoaMutatorStatements(
    std::vector<Expr> &statements,
    std::unordered_map<std::string, semantics::BindingInfo> bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &vectorReturnDefinitions,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaCollectionReturnDefinitions,
    const std::string &definitionNamespace,
    bool preservePushHelper,
    bool preserveReserveHelper,
    bool preserveVectorPushHelper,
    bool preserveVectorReserveHelper) {
  for (Expr &stmt : statements) {
    rewriteBuiltinSoaMutatorExpr(
        stmt,
        bindings,
        vectorReturnDefinitions,
        soaCollectionReturnDefinitions,
        definitionNamespace,
        preservePushHelper,
        preserveReserveHelper,
        preserveVectorPushHelper,
        preserveVectorReserveHelper);
    if (!stmt.bodyArguments.empty()) {
      auto bodyBindings = bindings;
      rewriteBuiltinSoaMutatorStatements(
          stmt.bodyArguments,
          bodyBindings,
          vectorReturnDefinitions,
          soaCollectionReturnDefinitions,
          definitionNamespace,
          preservePushHelper,
          preserveReserveHelper,
          preserveVectorPushHelper,
          preserveVectorReserveHelper);
    }
    if (stmt.isBinding) {
      if (auto vectorBinding = extractBuiltinVectorBinding(stmt); vectorBinding.has_value()) {
        bindings[stmt.name] = *vectorBinding;
      } else if (auto soaBinding = extractBuiltinSoaVectorBinding(stmt); soaBinding.has_value()) {
        bindings[stmt.name] = *soaBinding;
      }
    }
  }
}

void rewriteBuiltinSoaMutatorExpr(
    Expr &expr,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &vectorReturnDefinitions,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaCollectionReturnDefinitions,
    const std::string &definitionNamespace,
    bool preservePushHelper,
    bool preserveReserveHelper,
    bool preserveVectorPushHelper,
    bool preserveVectorReserveHelper) {
  auto findBuiltinVectorValueBinding = [&](const Expr &candidate) -> std::optional<semantics::BindingInfo> {
    if (candidate.kind == Expr::Kind::Name) {
      auto bindingIt = bindings.find(candidate.name);
      if (bindingIt != bindings.end() && isBuiltinVectorBinding(bindingIt->second)) {
        return bindingIt->second;
      }
      return std::nullopt;
    }
    if (candidate.kind != Expr::Kind::Call || candidate.isBinding) {
      return std::nullopt;
    }
    std::string collectionName;
    if (semantics::getBuiltinCollectionName(candidate, collectionName) &&
        collectionName == "vector" &&
        candidate.templateArgs.size() == 1) {
      semantics::BindingInfo binding;
      binding.typeName = "vector";
      binding.typeTemplateArg = candidate.templateArgs.front();
      return binding;
    }
    for (const std::string &candidatePath : candidateDefinitionPaths(candidate, definitionNamespace)) {
      auto returnIt = vectorReturnDefinitions.find(candidatePath);
      if (returnIt != vectorReturnDefinitions.end() && isBuiltinVectorBinding(returnIt->second)) {
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
              bindingTypeText(bindingIt->second), "vector");
        }
      }
      std::string accessName;
      if (semantics::getBuiltinArrayAccessName(derefTarget, accessName) &&
          derefTarget.args.size() == 2 &&
          derefTarget.args.front().kind == Expr::Kind::Name) {
        auto bindingIt = bindings.find(derefTarget.args.front().name);
        if (bindingIt != bindings.end()) {
          return extractBuiltinCollectionBindingFromWrappedTypeText(
              bindingTypeText(bindingIt->second), "vector");
        }
      }
    }
    return std::nullopt;
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
    rewriteBuiltinSoaMutatorExpr(
        arg,
        bindings,
        vectorReturnDefinitions,
        soaCollectionReturnDefinitions,
        definitionNamespace,
        preservePushHelper,
        preserveReserveHelper,
        preserveVectorPushHelper,
        preserveVectorReserveHelper);
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
  std::unordered_map<std::string, semantics::BindingInfo> vectorReturnDefinitions;
  std::unordered_map<std::string, semantics::BindingInfo> soaCollectionReturnDefinitions;
  for (const Definition &def : program.definitions) {
    if (auto binding = extractBuiltinVectorReturnBinding(def); binding.has_value()) {
      vectorReturnDefinitions[def.fullPath] = *binding;
      const size_t slash = def.fullPath.find_last_of('/');
      if (slash != std::string::npos && slash + 1 < def.fullPath.size()) {
        vectorReturnDefinitions[def.fullPath.substr(slash + 1)] = *binding;
      }
    }
    if (auto binding = extractBuiltinSoaVectorReturnBinding(def); binding.has_value()) {
      soaCollectionReturnDefinitions[def.fullPath] = *binding;
      const size_t slash = def.fullPath.find_last_of('/');
      if (slash != std::string::npos && slash + 1 < def.fullPath.size()) {
        soaCollectionReturnDefinitions[def.fullPath.substr(slash + 1)] = *binding;
      }
    }
  }
  const bool preservePushHelper = hasVisibleRootSoaHelper(program, "push");
  const bool preserveReserveHelper = hasVisibleRootSoaHelper(program, "reserve");
  const bool preserveVectorPushHelper =
      hasVisibleRootSoaHelperForReceiverType(program, "push", "vector");
  const bool preserveVectorReserveHelper =
      hasVisibleRootSoaHelperForReceiverType(program, "reserve", "vector");
  for (Definition &def : program.definitions) {
    std::unordered_map<std::string, semantics::BindingInfo> bindings;
    for (const Expr &param : def.parameters) {
      if (auto vectorBinding = extractBuiltinVectorBinding(param); vectorBinding.has_value()) {
        bindings[param.name] = *vectorBinding;
      } else if (auto soaBinding = extractBuiltinSoaVectorBinding(param); soaBinding.has_value()) {
        bindings[param.name] = *soaBinding;
      }
    }
    std::string definitionNamespace;
    const size_t slash = def.fullPath.find_last_of('/');
    if (slash != std::string::npos && slash > 0) {
      definitionNamespace = def.fullPath.substr(0, slash);
    }
    rewriteBuiltinSoaMutatorStatements(
        def.statements,
        bindings,
        vectorReturnDefinitions,
        soaCollectionReturnDefinitions,
        definitionNamespace,
        preservePushHelper,
        preserveReserveHelper,
        preserveVectorPushHelper,
        preserveVectorReserveHelper);
    if (def.returnExpr.has_value()) {
      rewriteBuiltinSoaMutatorExpr(
          *def.returnExpr,
          bindings,
          vectorReturnDefinitions,
          soaCollectionReturnDefinitions,
          definitionNamespace,
          preservePushHelper,
          preserveReserveHelper,
          preserveVectorPushHelper,
          preserveVectorReserveHelper);
    }
  }
  return true;
}

} // namespace primec
