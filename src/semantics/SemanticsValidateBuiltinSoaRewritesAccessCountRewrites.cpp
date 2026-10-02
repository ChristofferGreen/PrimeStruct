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

void rewriteBuiltinSoaAccessExpr(
    Expr &expr,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &vectorReturnDefinitions,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaCollectionReturnDefinitions,
    const std::string &definitionNamespace,
    bool preserveGetHelper,
    bool preserveGetRefHelper,
    bool preserveRefHelper,
    bool preserveRefRefHelper,
    const std::unordered_set<std::string> &visiblePublicSoaHelpers);

void rewriteBuiltinSoaAccessStatements(
    std::vector<Expr> &statements,
    std::unordered_map<std::string, semantics::BindingInfo> bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &vectorReturnDefinitions,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaCollectionReturnDefinitions,
    const std::string &definitionNamespace,
    bool preserveGetHelper,
    bool preserveGetRefHelper,
    bool preserveRefHelper,
    bool preserveRefRefHelper,
    const std::unordered_set<std::string> &visiblePublicSoaHelpers) {
  for (Expr &stmt : statements) {
    rewriteBuiltinSoaAccessExpr(
        stmt,
        bindings,
        vectorReturnDefinitions,
        soaCollectionReturnDefinitions,
        definitionNamespace,
        preserveGetHelper,
        preserveGetRefHelper,
        preserveRefHelper,
        preserveRefRefHelper,
        visiblePublicSoaHelpers);
    if (!stmt.bodyArguments.empty()) {
      auto bodyBindings = bindings;
      rewriteBuiltinSoaAccessStatements(
          stmt.bodyArguments,
          bodyBindings,
          vectorReturnDefinitions,
          soaCollectionReturnDefinitions,
          definitionNamespace,
          preserveGetHelper,
          preserveGetRefHelper,
          preserveRefHelper,
          preserveRefRefHelper,
          visiblePublicSoaHelpers);
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

void rewriteBuiltinSoaAccessExpr(
    Expr &expr,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &vectorReturnDefinitions,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaCollectionReturnDefinitions,
    const std::string &definitionNamespace,
    bool preserveGetHelper,
    bool preserveGetRefHelper,
    bool preserveRefHelper,
    bool preserveRefRefHelper,
    const std::unordered_set<std::string> &visiblePublicSoaHelpers) {
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
  auto findBuiltinSoaValueBinding = [&](const Expr &candidate)
      -> std::optional<BuiltinSoaReceiverBindingInfo> {
    if (candidate.kind == Expr::Kind::Name) {
      auto bindingIt = bindings.find(candidate.name);
      if (bindingIt != bindings.end()) {
        return extractBuiltinSoaReceiverBinding(bindingIt->second);
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
      return BuiltinSoaReceiverBindingInfo{binding, false};
    }
    for (const std::string &candidatePath : candidateDefinitionPaths(candidate, definitionNamespace)) {
      auto returnIt = soaCollectionReturnDefinitions.find(candidatePath);
      if (returnIt != soaCollectionReturnDefinitions.end()) {
        return extractBuiltinSoaReceiverBinding(returnIt->second);
      }
    }
    if (!candidate.isMethodCall &&
        semantics::isSimpleCallName(candidate, "dereference") &&
        candidate.args.size() == 1) {
      const Expr &derefTarget = candidate.args.front();
      if (derefTarget.kind == Expr::Kind::Name) {
        auto bindingIt = bindings.find(derefTarget.name);
        if (bindingIt != bindings.end()) {
          if (auto binding = extractBuiltinCollectionBindingFromWrappedTypeText(
                  bindingTypeText(bindingIt->second),
                  semantics::internalSoaCollectionTypeName());
              binding.has_value()) {
            return BuiltinSoaReceiverBindingInfo{*binding, false};
          }
        }
      }
      std::string accessName;
      if (semantics::getBuiltinArrayAccessName(derefTarget, accessName) &&
          derefTarget.args.size() == 2 &&
          derefTarget.args.front().kind == Expr::Kind::Name) {
        auto bindingIt = bindings.find(derefTarget.args.front().name);
        if (bindingIt != bindings.end()) {
          if (auto binding = extractBuiltinCollectionBindingFromWrappedTypeText(
                  bindingTypeText(bindingIt->second),
                  semantics::internalSoaCollectionTypeName());
              binding.has_value()) {
            return BuiltinSoaReceiverBindingInfo{*binding, false};
          }
        }
      }
    }
    return std::nullopt;
  };

  for (Expr &arg : expr.args) {
    rewriteBuiltinSoaAccessExpr(
        arg,
        bindings,
        vectorReturnDefinitions,
        soaCollectionReturnDefinitions,
        definitionNamespace,
        preserveGetHelper,
        preserveGetRefHelper,
        preserveRefHelper,
        preserveRefRefHelper,
        visiblePublicSoaHelpers);
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
    if (auto binding = extractBuiltinSoaVectorOrBorrowedReturnBinding(def); binding.has_value()) {
      soaCollectionReturnDefinitions[def.fullPath] = *binding;
      const size_t slash = def.fullPath.find_last_of('/');
      if (slash != std::string::npos && slash + 1 < def.fullPath.size()) {
        soaCollectionReturnDefinitions[def.fullPath.substr(slash + 1)] = *binding;
      }
    }
  }
  const bool preserveGetHelper = hasVisibleRootSoaHelper(program, "get");
  const bool preserveGetRefHelper = hasVisibleRootSoaHelper(program, collection_helpers::kGetRef);
  const bool preserveRefHelper = hasVisibleRootSoaHelper(program, "ref");
  const bool preserveRefRefHelper = hasVisibleRootSoaHelper(program, collection_helpers::kRefRef);
  std::unordered_set<std::string> visiblePublicSoaHelpers;
  for (std::string_view helperName : {collection_helpers::kGetRef, collection_helpers::kRefRef}) {
    if (hasVisiblePublicSoaHelperDefinition(program, helperName)) {
      visiblePublicSoaHelpers.insert(std::string(helperName));
    }
  }
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
    rewriteBuiltinSoaAccessStatements(
        def.statements,
        bindings,
        vectorReturnDefinitions,
        soaCollectionReturnDefinitions,
        definitionNamespace,
        preserveGetHelper,
        preserveGetRefHelper,
        preserveRefHelper,
        preserveRefRefHelper,
        visiblePublicSoaHelpers);
    if (def.returnExpr.has_value()) {
      rewriteBuiltinSoaAccessExpr(
          *def.returnExpr,
          bindings,
          vectorReturnDefinitions,
          soaCollectionReturnDefinitions,
          definitionNamespace,
          preserveGetHelper,
          preserveGetRefHelper,
          preserveRefHelper,
          preserveRefRefHelper,
          visiblePublicSoaHelpers);
    }
  }
  return true;
}

void rewriteBuiltinSoaCountExpr(
    Expr &expr,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &vectorReturnDefinitions,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaCollectionReturnDefinitions,
    const std::string &definitionNamespace,
    bool preserveCountHelper,
    bool preserveCountRefHelper,
    const std::unordered_set<std::string> &visiblePublicSoaHelpers);

void rewriteBuiltinSoaCountStatements(
    std::vector<Expr> &statements,
    std::unordered_map<std::string, semantics::BindingInfo> bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &vectorReturnDefinitions,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaCollectionReturnDefinitions,
    const std::string &definitionNamespace,
    bool preserveCountHelper,
    bool preserveCountRefHelper,
    const std::unordered_set<std::string> &visiblePublicSoaHelpers) {
  for (Expr &stmt : statements) {
    rewriteBuiltinSoaCountExpr(
        stmt,
        bindings,
        vectorReturnDefinitions,
        soaCollectionReturnDefinitions,
        definitionNamespace,
        preserveCountHelper,
        preserveCountRefHelper,
        visiblePublicSoaHelpers);
    if (!stmt.bodyArguments.empty()) {
      auto bodyBindings = bindings;
      rewriteBuiltinSoaCountStatements(
          stmt.bodyArguments,
          bodyBindings,
          vectorReturnDefinitions,
          soaCollectionReturnDefinitions,
          definitionNamespace,
          preserveCountHelper,
          preserveCountRefHelper,
          visiblePublicSoaHelpers);
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

void rewriteBuiltinSoaCountExpr(
    Expr &expr,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &vectorReturnDefinitions,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaCollectionReturnDefinitions,
    const std::string &definitionNamespace,
    bool preserveCountHelper,
    bool preserveCountRefHelper,
    const std::unordered_set<std::string> &visiblePublicSoaHelpers) {
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
  auto findBuiltinSoaValueBinding = [&](const Expr &candidate)
      -> std::optional<BuiltinSoaReceiverBindingInfo> {
    if (candidate.kind == Expr::Kind::Name) {
      auto bindingIt = bindings.find(candidate.name);
      if (bindingIt != bindings.end()) {
        return extractBuiltinSoaReceiverBinding(bindingIt->second);
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
      return BuiltinSoaReceiverBindingInfo{binding, false};
    }
    for (const std::string &candidatePath : candidateDefinitionPaths(candidate, definitionNamespace)) {
      auto returnIt = soaCollectionReturnDefinitions.find(candidatePath);
      if (returnIt != soaCollectionReturnDefinitions.end()) {
        return extractBuiltinSoaReceiverBinding(returnIt->second);
      }
    }
    if (!candidate.isMethodCall &&
        semantics::isSimpleCallName(candidate, "dereference") &&
        candidate.args.size() == 1) {
      const Expr &derefTarget = candidate.args.front();
      if (derefTarget.kind == Expr::Kind::Name) {
        auto bindingIt = bindings.find(derefTarget.name);
        if (bindingIt != bindings.end()) {
          if (auto binding = extractBuiltinCollectionBindingFromWrappedTypeText(
                  bindingTypeText(bindingIt->second),
                  semantics::internalSoaCollectionTypeName());
              binding.has_value()) {
            return BuiltinSoaReceiverBindingInfo{*binding, false};
          }
        }
      }
      std::string accessName;
      if (semantics::getBuiltinArrayAccessName(derefTarget, accessName) &&
          derefTarget.args.size() == 2 &&
          derefTarget.args.front().kind == Expr::Kind::Name) {
        auto bindingIt = bindings.find(derefTarget.args.front().name);
        if (bindingIt != bindings.end()) {
          if (auto binding = extractBuiltinCollectionBindingFromWrappedTypeText(
                  bindingTypeText(bindingIt->second),
                  semantics::internalSoaCollectionTypeName());
              binding.has_value()) {
            return BuiltinSoaReceiverBindingInfo{*binding, false};
          }
        }
      }
    }
    return std::nullopt;
  };

  for (Expr &arg : expr.args) {
    rewriteBuiltinSoaCountExpr(
        arg,
        bindings,
        vectorReturnDefinitions,
        soaCollectionReturnDefinitions,
        definitionNamespace,
        preserveCountHelper,
        preserveCountRefHelper,
        visiblePublicSoaHelpers);
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
    if (auto binding = extractBuiltinSoaVectorOrBorrowedReturnBinding(def); binding.has_value()) {
      soaCollectionReturnDefinitions[def.fullPath] = *binding;
      const size_t slash = def.fullPath.find_last_of('/');
      if (slash != std::string::npos && slash + 1 < def.fullPath.size()) {
        soaCollectionReturnDefinitions[def.fullPath.substr(slash + 1)] = *binding;
      }
    }
  }
  const bool preserveCountHelper = hasVisibleRootSoaHelper(program, "count");
  const bool preserveCountRefHelper = hasVisibleRootSoaHelper(program, collection_helpers::kCountRef);
  std::unordered_set<std::string> visiblePublicSoaHelpers;
  if (hasVisiblePublicSoaHelperDefinition(program, collection_helpers::kCountRef)) {
    visiblePublicSoaHelpers.insert(collection_helpers::kCountRef);
  }
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
    rewriteBuiltinSoaCountStatements(
        def.statements,
        bindings,
        vectorReturnDefinitions,
        soaCollectionReturnDefinitions,
        definitionNamespace,
        preserveCountHelper,
        preserveCountRefHelper,
        visiblePublicSoaHelpers);
    if (def.returnExpr.has_value()) {
      rewriteBuiltinSoaCountExpr(
          *def.returnExpr,
          bindings,
          vectorReturnDefinitions,
          soaCollectionReturnDefinitions,
          definitionNamespace,
          preserveCountHelper,
          preserveCountRefHelper,
          visiblePublicSoaHelpers);
    }
  }
  return true;
}

} // namespace primec
