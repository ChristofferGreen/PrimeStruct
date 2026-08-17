// soa-surface-audit: exempt
#include "SemanticsValidateBuiltinSoaRewrites.h"

#include "SemanticsHelpers.h"
#include "SemanticsValidateBuiltinSoaMetadata.h"
#include "SemanticsValidateSoaBindingExtraction.h"

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace primec {

void rewriteBuiltinSoaConversionMethodExpr(
    Expr &expr,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &vectorReturnDefinitions,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaCollectionReturnDefinitions,
    const std::string &definitionNamespace);

void rewriteBuiltinSoaConversionMethodStatements(
    std::vector<Expr> &statements,
    std::unordered_map<std::string, semantics::BindingInfo> bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &vectorReturnDefinitions,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaCollectionReturnDefinitions,
    const std::string &definitionNamespace) {
  for (Expr &stmt : statements) {
    rewriteBuiltinSoaConversionMethodExpr(
        stmt, bindings, vectorReturnDefinitions, soaCollectionReturnDefinitions, definitionNamespace);
    if (!stmt.bodyArguments.empty()) {
      auto bodyBindings = bindings;
      rewriteBuiltinSoaConversionMethodStatements(
          stmt.bodyArguments, bodyBindings, vectorReturnDefinitions, soaCollectionReturnDefinitions, definitionNamespace);
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

void rewriteBuiltinSoaConversionMethodExpr(
    Expr &expr,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &vectorReturnDefinitions,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaCollectionReturnDefinitions,
    const std::string &definitionNamespace) {
  for (Expr &arg : expr.args) {
    rewriteBuiltinSoaConversionMethodExpr(
        arg, bindings, vectorReturnDefinitions, soaCollectionReturnDefinitions, definitionNamespace);
  }
  if (expr.kind != Expr::Kind::Call || !expr.isMethodCall || expr.args.empty() ||
      expr.args.front().kind == Expr::Kind::Literal) {
    return;
  }
  const std::string helperName = builtinSoaConversionMethodName(expr.name);
  if (helperName.empty()) {
    return;
  }
  auto matchesBuiltinReceiverBinding = [&](const semantics::BindingInfo &binding) {
    if (helperName == "to_soa") {
      return isBuiltinVectorBinding(binding);
    }
    if (helperName == "to_aos") {
      return isBuiltinSoaVectorBinding(binding);
    }
    return false;
  };
  const auto &returnDefinitions =
      helperName == "to_soa" ? vectorReturnDefinitions : soaCollectionReturnDefinitions;
  std::optional<semantics::BindingInfo> receiverBinding;
  const Expr &receiver = expr.args.front();
  if (receiver.kind == Expr::Kind::Name) {
    auto bindingIt = bindings.find(receiver.name);
    if (bindingIt != bindings.end() && matchesBuiltinReceiverBinding(bindingIt->second)) {
      receiverBinding = bindingIt->second;
    }
  } else if (receiver.kind == Expr::Kind::Call && !receiver.isBinding) {
    std::vector<std::string> candidatePaths;
    if (!receiver.name.empty() && receiver.name.front() == '/') {
      candidatePaths.push_back(receiver.name);
    } else {
      if (!receiver.namespacePrefix.empty()) {
        candidatePaths.push_back(receiver.namespacePrefix + "/" + receiver.name);
      }
      if (!definitionNamespace.empty()) {
        candidatePaths.push_back(definitionNamespace + "/" + receiver.name);
      }
      candidatePaths.push_back("/" + receiver.name);
      candidatePaths.push_back(receiver.name);
    }
    for (const std::string &candidatePath : candidatePaths) {
      auto returnIt = returnDefinitions.find(candidatePath);
      if (returnIt != returnDefinitions.end() &&
          matchesBuiltinReceiverBinding(returnIt->second)) {
        receiverBinding = returnIt->second;
        break;
      }
    }
  }
  if (!receiverBinding.has_value()) {
    return;
  }

  expr.isMethodCall = false;
  expr.isFieldAccess = false;
  expr.name = helperName;
  expr.namespacePrefix.clear();
}

bool rewriteBuiltinSoaConversionMethods(Program &program, std::string &error) {
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
    rewriteBuiltinSoaConversionMethodStatements(
        def.statements, bindings, vectorReturnDefinitions, soaCollectionReturnDefinitions, definitionNamespace);
    if (def.returnExpr.has_value()) {
      rewriteBuiltinSoaConversionMethodExpr(
          *def.returnExpr, bindings, vectorReturnDefinitions, soaCollectionReturnDefinitions, definitionNamespace);
    }
  }
  return true;
}

void rewriteBuiltinSoaToAosCallExpr(
    Expr &expr,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &vectorReturnDefinitions,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaCollectionReturnDefinitions,
    const std::string &definitionNamespace,
    bool preserveVisibleRootSoaHelper,
    bool preserveVisibleRootVectorHelper);

void rewriteBuiltinSoaToAosCallStatements(
    std::vector<Expr> &statements,
    std::unordered_map<std::string, semantics::BindingInfo> bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &vectorReturnDefinitions,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaCollectionReturnDefinitions,
    const std::string &definitionNamespace,
    bool preserveVisibleRootSoaHelper,
    bool preserveVisibleRootVectorHelper) {
  for (Expr &stmt : statements) {
    rewriteBuiltinSoaToAosCallExpr(
        stmt,
        bindings,
        vectorReturnDefinitions,
        soaCollectionReturnDefinitions,
        definitionNamespace,
        preserveVisibleRootSoaHelper,
        preserveVisibleRootVectorHelper);
    if (!stmt.bodyArguments.empty()) {
      auto bodyBindings = bindings;
      rewriteBuiltinSoaToAosCallStatements(
          stmt.bodyArguments,
          bodyBindings,
          vectorReturnDefinitions,
          soaCollectionReturnDefinitions,
          definitionNamespace,
          preserveVisibleRootSoaHelper,
          preserveVisibleRootVectorHelper);
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

void rewriteBuiltinSoaToAosCallExpr(
    Expr &expr,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &vectorReturnDefinitions,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaCollectionReturnDefinitions,
    const std::string &definitionNamespace,
    bool preserveVisibleRootSoaHelper,
    bool preserveVisibleRootVectorHelper) {
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
        builtinSoaConversionMethodName(candidate.name) == "to_soa" &&
        candidate.args.size() == 1) {
      auto vectorBinding = findBuiltinVectorValueBinding(candidate.args.front());
      if (!vectorBinding.has_value() || vectorBinding->typeTemplateArg.empty()) {
        return std::nullopt;
      }
      semantics::BindingInfo binding;
      binding.typeName = semantics::internalSoaCollectionTypeName();
      binding.typeTemplateArg = vectorBinding->typeTemplateArg;
      return BuiltinSoaReceiverBindingInfo{binding, false};
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
    rewriteBuiltinSoaToAosCallExpr(
        arg,
        bindings,
        vectorReturnDefinitions,
        soaCollectionReturnDefinitions,
        definitionNamespace,
        preserveVisibleRootSoaHelper,
        preserveVisibleRootVectorHelper);
  }
  if (expr.kind != Expr::Kind::Call ||
      expr.args.size() != 1 ||
      !expr.templateArgs.empty() ||
      semantics::hasNamedArguments(expr.argNames) ||
      expr.hasBodyArguments ||
      !expr.bodyArguments.empty() ||
      builtinSoaConversionMethodName(expr.name) != "to_aos") {
    return;
  }

  const bool hasBuiltinSoaReceiver =
      findBuiltinSoaValueBinding(expr.args.front()).has_value();
  const bool hasBuiltinVectorReceiver =
      findBuiltinVectorValueBinding(expr.args.front()).has_value();
  if (!hasBuiltinSoaReceiver && !hasBuiltinVectorReceiver) {
    return;
  }
  const bool isExplicitRootToAosSurface =
      !expr.name.empty() && expr.name.front() == '/';
  if (isExplicitRootToAosSurface) {
    if (expr.isMethodCall &&
        ((hasBuiltinSoaReceiver && preserveVisibleRootSoaHelper) ||
         (hasBuiltinVectorReceiver && preserveVisibleRootVectorHelper))) {
      expr.isMethodCall = false;
      expr.isFieldAccess = false;
      expr.name = "/to_aos";
      expr.namespacePrefix.clear();
      expr.templateArgs.clear();
    }
    return;
  }
  if (hasBuiltinVectorReceiver && preserveVisibleRootVectorHelper && expr.isMethodCall) {
    expr.isMethodCall = false;
    expr.isFieldAccess = false;
    expr.name = "/to_aos";
    expr.namespacePrefix.clear();
    expr.templateArgs.clear();
    return;
  }
  if ((hasBuiltinSoaReceiver && preserveVisibleRootSoaHelper) ||
      (hasBuiltinVectorReceiver && preserveVisibleRootVectorHelper)) {
    return;
  }

  expr.isMethodCall = false;
  expr.isFieldAccess = false;
  expr.name = semantics::compatibilitySoaHelperTargetPath("to_aos");
  expr.namespacePrefix.clear();
  expr.templateArgs.clear();
}

bool rewriteBuiltinSoaToAosCalls(Program &program, std::string &error) {
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
  const bool preserveVisibleRootSoaHelper =
      hasVisibleRootSoaHelperForReceiverType(
          program, "to_aos", semantics::internalSoaCollectionTypeName());
  const bool preserveVisibleRootVectorHelper =
      hasVisibleRootSoaHelperForReceiverType(program, "to_aos", "vector");
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
    rewriteBuiltinSoaToAosCallStatements(
        def.statements,
        bindings,
        vectorReturnDefinitions,
        soaCollectionReturnDefinitions,
        definitionNamespace,
        preserveVisibleRootSoaHelper,
        preserveVisibleRootVectorHelper);
    if (def.returnExpr.has_value()) {
      rewriteBuiltinSoaToAosCallExpr(
          *def.returnExpr,
          bindings,
          vectorReturnDefinitions,
          soaCollectionReturnDefinitions,
          definitionNamespace,
          preserveVisibleRootSoaHelper,
          preserveVisibleRootVectorHelper);
    }
  }
  return true;
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
       (resolvedHelperName == "get_ref" && preserveGetRefHelper) ||
       (resolvedHelperName == "ref" && preserveRefHelper) ||
       (resolvedHelperName == "ref_ref" && preserveRefRefHelper))) {
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
  const bool preserveGetRefHelper = hasVisibleRootSoaHelper(program, "get_ref");
  const bool preserveRefHelper = hasVisibleRootSoaHelper(program, "ref");
  const bool preserveRefRefHelper = hasVisibleRootSoaHelper(program, "ref_ref");
  std::unordered_set<std::string> visiblePublicSoaHelpers;
  for (std::string_view helperName : {"get_ref", "ref_ref"}) {
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
       (resolvedHelperName == "count_ref" && preserveCountRefHelper))) {
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
  const bool preserveCountRefHelper = hasVisibleRootSoaHelper(program, "count_ref");
  std::unordered_set<std::string> visiblePublicSoaHelpers;
  if (hasVisiblePublicSoaHelperDefinition(program, "count_ref")) {
    visiblePublicSoaHelpers.insert("count_ref");
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
