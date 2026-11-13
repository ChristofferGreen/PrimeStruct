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

void rewriteBuiltinSoaConversionMethodExpr(Expr &expr,
                                           const SoaBindingMap &bindings,
                                           const BuiltinSoaRewriteScope &scope) {
  const SoaBindingMap &vectorReturnDefinitions = scope.vectorReturnDefinitions;
  const SoaBindingMap &soaCollectionReturnDefinitions = scope.soaCollectionReturnDefinitions;
  const std::string &definitionNamespace = scope.definitionNamespace;
  for (Expr &arg : expr.args) {
    rewriteBuiltinSoaConversionMethodExpr(arg, bindings, scope);
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
  rewriteBuiltinSoaDefinitions(
      program,
      extractBuiltinSoaVectorReturnBinding,
      [&](Expr &expr, const SoaBindingMap &bindings, const BuiltinSoaRewriteScope &scope) {
        rewriteBuiltinSoaConversionMethodExpr(expr, bindings, scope);
      });
  return true;
}

struct BuiltinSoaToAosPreserveFlags {
  bool visibleRootSoaHelper = false;
  bool visibleRootVectorHelper = false;
};

void rewriteBuiltinSoaToAosCallExpr(Expr &expr,
                                    const SoaBindingMap &bindings,
                                    const BuiltinSoaRewriteScope &scope,
                                    const BuiltinSoaToAosPreserveFlags &flags) {
  const SoaBindingMap &soaCollectionReturnDefinitions = scope.soaCollectionReturnDefinitions;
  const std::string &definitionNamespace = scope.definitionNamespace;
  const bool preserveVisibleRootSoaHelper = flags.visibleRootSoaHelper;
  const bool preserveVisibleRootVectorHelper = flags.visibleRootVectorHelper;
  auto findBuiltinVectorValueBinding = [&](const Expr &candidate) {
    return primec::findBuiltinVectorValueBinding(candidate, bindings, scope);
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
    rewriteBuiltinSoaToAosCallExpr(arg, bindings, scope, flags);
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
  const BuiltinSoaToAosPreserveFlags flags{
      hasVisibleRootSoaHelperForReceiverType(
          program, "to_aos", semantics::internalSoaCollectionTypeName()),
      hasVisibleRootSoaHelperForReceiverType(program, "to_aos", "vector")};
  rewriteBuiltinSoaDefinitions(
      program,
      extractBuiltinSoaVectorReturnBinding,
      [&](Expr &expr, const SoaBindingMap &bindings, const BuiltinSoaRewriteScope &scope) {
        rewriteBuiltinSoaToAosCallExpr(expr, bindings, scope, flags);
      });
  return true;
}

} // namespace primec
