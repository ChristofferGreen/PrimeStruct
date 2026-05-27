#include "SemanticsValidator.h"
#include "SemanticsValidatorInferCollectionCompatibilityInternal.h"

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace primec::semantics {

bool SemanticsValidator::validateExprCollectionLiteralBuiltins(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &expr,
    bool &handledOut) {
  auto failCollectionLiteralDiagnostic = [&](std::string message) -> bool {
    return failExprDiagnostic(expr, std::move(message));
  };
  handledOut = false;
  std::string builtinName;
  if (!getBuiltinCollectionName(expr, builtinName)) {
    return true;
  }
  if (builtinName == "map") {
    return true;
  }

  handledOut = true;
  auto collectionLiteralDiagnosticSubject = [&]() -> std::string {
    if (builtinName == "vector") {
      return "collection literal";
    }
    return builtinName + " literal";
  };
  const BuiltinCollectionDispatchResolverAdapters builtinCollectionDispatchResolverAdapters{
      .resolveBindingTarget =
          [&](const Expr &target, BindingInfo &bindingOut) -> bool {
            if (!(target.kind == Expr::Kind::Call && target.isFieldAccess &&
                  target.args.size() == 1)) {
              return false;
            }
            return resolveStructFieldBinding(params, locals, target.args.front(),
                                             target.name, bindingOut);
          },
      .inferCallBinding =
          [&](const Expr &target, BindingInfo &bindingOut) -> bool {
            if (target.kind != Expr::Kind::Call) {
              return false;
            }
            auto defIt = defMap_.find(resolveCalleePath(target));
            return defIt != defMap_.end() && defIt->second != nullptr &&
                   inferDefinitionReturnBinding(*defIt->second, bindingOut);
          }};
  const BuiltinCollectionDispatchResolvers builtinCollectionDispatchResolvers =
      makeBuiltinCollectionDispatchResolvers(params, locals,
                                            builtinCollectionDispatchResolverAdapters);
  if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
    return failCollectionLiteralDiagnostic(collectionLiteralDiagnosticSubject() +
                                          " does not accept block arguments");
  }
  if (builtinName == "soa" "_vector") {
    if (expr.templateArgs.size() != 1) {
      return failCollectionLiteralDiagnostic(
          "soa literal requires exactly one template argument");
    }
    if (!isSoaVectorStructElementType(expr.templateArgs.front(),
                                      expr.namespacePrefix, structNames_,
                                      importAliases_)) {
      return failCollectionLiteralDiagnostic("soa literal requires struct element type");
    }
    if (!validateSoaVectorElementFieldEnvelopes(expr.templateArgs.front(),
                                                expr.namespacePrefix)) {
      return false;
    }
    if (!expr.args.empty() && currentValidationState_.context.activeEffects.count("heap_alloc") == 0) {
      return failCollectionLiteralDiagnostic("soa literal requires heap_alloc effect");
    }
  }
  if (builtinName == "vector" && !expr.args.empty()) {
    if (currentValidationState_.context.activeEffects.count("heap_alloc") == 0) {
      return failCollectionLiteralDiagnostic(collectionLiteralDiagnosticSubject() +
                                            " requires heap_alloc effect");
    }
  }
  if (expr.templateArgs.size() != 1) {
    if (builtinName == "array" && expr.templateArgs.size() > 1) {
      return failCollectionLiteralDiagnostic(
          "array<T, N> is unsupported; use array<T> (runtime-count array)");
    }
    return failCollectionLiteralDiagnostic(
        collectionLiteralDiagnosticSubject() +
        " requires exactly one template argument");
  }
  for (const auto &arg : expr.args) {
    if (!validateExpr(params, locals, arg)) {
      return false;
    }
  }
  auto resolveOwnershipContext = [&](std::string &namespacePrefixOut,
                                     const std::vector<std::string> *&definitionTemplateArgsOut) {
    definitionTemplateArgsOut = nullptr;
    namespacePrefixOut = expr.namespacePrefix;
    const Definition *currentDef = nullptr;
    if (!currentValidationState_.context.definitionPath.empty()) {
      auto currentDefIt = defMap_.find(currentValidationState_.context.definitionPath);
      if (currentDefIt != defMap_.end()) {
        currentDef = currentDefIt->second;
      }
    }
    if (currentDef != nullptr) {
      definitionTemplateArgsOut = &currentDef->templateArgs;
      if (namespacePrefixOut.empty()) {
        namespacePrefixOut = currentDef->namespacePrefix;
      }
    }
  };
  if ((builtinName == "array" || builtinName == "vector" ||
       builtinName == "soa" "_vector") &&
      !expr.templateArgs.empty()) {
    const std::string &elemType = expr.templateArgs.front();
    const std::vector<std::string> *definitionTemplateArgs = nullptr;
    std::string definitionNamespacePrefix;
    resolveOwnershipContext(definitionNamespacePrefix, definitionTemplateArgs);
    const std::string experimentalVectorContextPrefix =
        legacyExperimentalVectorCompatibilityPrefix();
    std::string experimentalVectorContextRoot =
        experimentalVectorContextPrefix;
    if (!experimentalVectorContextRoot.empty() &&
        experimentalVectorContextRoot.back() == '/') {
      experimentalVectorContextRoot.pop_back();
    }
    const bool isCollectionVectorContext =
        currentValidationState_.context.definitionPath.rfind(
            experimentalVectorContextPrefix, 0) == 0 ||
        definitionNamespacePrefix.rfind(experimentalVectorContextRoot, 0) == 0;
    if (builtinName == "vector" && !expr.args.empty() &&
        !isCollectionVectorContext) {
      std::unordered_set<std::string> visitingStructs;
      if (!isRelocationTrivialContainerElementType(
              elemType, definitionNamespacePrefix, definitionTemplateArgs, visitingStructs)) {
        return failCollectionLiteralDiagnostic(
            collectionLiteralDiagnosticSubject() +
            " requires relocation-trivial collection element type until container "
            "move/reallocation semantics are implemented: " +
            elemType);
      }
    }
    for (const auto &arg : expr.args) {
      if (!this->validateCollectionElementType(
              arg, elemType, collectionLiteralDiagnosticSubject() +
                                 " requires element type ",
              params, locals, builtinCollectionDispatchResolvers)) {
        return false;
      }
    }
  }
  return true;
}

} // namespace primec::semantics
