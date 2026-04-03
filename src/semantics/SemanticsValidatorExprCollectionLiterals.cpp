#include "SemanticsValidator.h"

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
  auto publishCollectionLiteralDiagnostic = [&]() -> bool {
    captureExprContext(expr);
    return publishCurrentStructuredDiagnosticNow();
  };
  handledOut = false;
  std::string builtinName;
  if (!getBuiltinCollectionName(expr, builtinName)) {
    return true;
  }

  handledOut = true;
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
    error_ = builtinName + " literal does not accept block arguments";
    return publishCollectionLiteralDiagnostic();
  }
  if (builtinName == "soa_vector") {
    if (expr.templateArgs.size() != 1) {
      error_ = "soa_vector literal requires exactly one template argument";
      return publishCollectionLiteralDiagnostic();
    }
    if (!isSoaVectorStructElementType(expr.templateArgs.front(),
                                      expr.namespacePrefix, structNames_,
                                      importAliases_)) {
      error_ = "soa_vector literal requires struct element type";
      return publishCollectionLiteralDiagnostic();
    }
    if (!validateSoaVectorElementFieldEnvelopes(expr.templateArgs.front(),
                                                expr.namespacePrefix)) {
      return false;
    }
    if (!expr.args.empty() && currentValidationContext_.activeEffects.count("heap_alloc") == 0) {
      error_ = "soa_vector literal requires heap_alloc effect";
      return publishCollectionLiteralDiagnostic();
    }
  }
  if (builtinName == "vector" && !expr.args.empty()) {
    if (currentValidationContext_.activeEffects.count("heap_alloc") == 0) {
      error_ = "vector literal requires heap_alloc effect";
      return publishCollectionLiteralDiagnostic();
    }
  }
  if (builtinName == "array" || builtinName == "vector" ||
      builtinName == "soa_vector") {
    if (expr.templateArgs.size() != 1) {
      if (builtinName == "array" && expr.templateArgs.size() > 1) {
        error_ =
            "array<T, N> is unsupported; use array<T> (runtime-count array)";
        return publishCollectionLiteralDiagnostic();
      }
      error_ = builtinName + " literal requires exactly one template argument";
      return publishCollectionLiteralDiagnostic();
    }
  } else {
    if (expr.templateArgs.size() != 2) {
      error_ = "map literal requires exactly two template arguments";
      return publishCollectionLiteralDiagnostic();
    }
    if (expr.args.size() % 2 != 0) {
      error_ = "map literal requires an even number of arguments";
      return publishCollectionLiteralDiagnostic();
    }
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
    if (!currentValidationContext_.definitionPath.empty()) {
      auto currentDefIt = defMap_.find(currentValidationContext_.definitionPath);
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
       builtinName == "soa_vector") &&
      !expr.templateArgs.empty()) {
    const std::string &elemType = expr.templateArgs.front();
    const std::vector<std::string> *definitionTemplateArgs = nullptr;
    std::string definitionNamespacePrefix;
    resolveOwnershipContext(definitionNamespacePrefix, definitionTemplateArgs);
    const bool isExperimentalVectorContext =
        currentValidationContext_.definitionPath.rfind("/std/collections/experimental_vector/", 0) == 0 ||
        definitionNamespacePrefix.rfind("/std/collections/experimental_vector", 0) == 0;
    if (builtinName == "vector" && !expr.args.empty() &&
        !isExperimentalVectorContext) {
      std::unordered_set<std::string> visitingStructs;
      if (!isRelocationTrivialContainerElementType(
              elemType, definitionNamespacePrefix, definitionTemplateArgs, visitingStructs)) {
        error_ =
            "vector literal requires relocation-trivial vector element type until container move/reallocation "
            "semantics are implemented: " +
            elemType;
        return publishCollectionLiteralDiagnostic();
      }
    }
    for (const auto &arg : expr.args) {
      if (!this->validateCollectionElementType(
              arg, elemType, builtinName + " literal requires element type ",
              params, locals, builtinCollectionDispatchResolvers)) {
        return false;
      }
    }
  }
  if (builtinName == "map" && expr.templateArgs.size() == 2) {
    if (!validateBuiltinMapKeyType(expr.templateArgs.front(), nullptr, error_)) {
      return false;
    }
    const std::vector<std::string> *definitionTemplateArgs = nullptr;
    std::string definitionNamespacePrefix;
    resolveOwnershipContext(definitionNamespacePrefix, definitionTemplateArgs);
    const std::string &keyType = expr.templateArgs[0];
    const std::string &valueType = expr.templateArgs[1];
    if (!expr.args.empty()) {
      std::unordered_set<std::string> visitingStructs;
      if (!isRelocationTrivialContainerElementType(
              keyType, definitionNamespacePrefix, definitionTemplateArgs, visitingStructs)) {
        error_ =
            "map literal requires relocation-trivial map key type until container move/reallocation semantics are "
            "implemented: " +
            keyType;
        return publishCollectionLiteralDiagnostic();
      }
      visitingStructs.clear();
      if (!isRelocationTrivialContainerElementType(
              valueType, definitionNamespacePrefix, definitionTemplateArgs, visitingStructs)) {
        error_ =
            "map literal requires relocation-trivial map value type until container move/reallocation semantics are "
            "implemented: " +
            valueType;
        return publishCollectionLiteralDiagnostic();
      }
    }
    for (size_t i = 0; i + 1 < expr.args.size(); i += 2) {
      if (!this->validateCollectionElementType(
              expr.args[i], keyType, "map literal requires key type ", params,
              locals, builtinCollectionDispatchResolvers)) {
        return false;
      }
      if (!this->validateCollectionElementType(
              expr.args[i + 1], valueType,
              "map literal requires value type ", params, locals,
              builtinCollectionDispatchResolvers)) {
        return false;
      }
    }
  }
  return true;
}

} // namespace primec::semantics
