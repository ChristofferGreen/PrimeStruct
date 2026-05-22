#include "SemanticsValidator.h"
#include "SemanticsValidatorInferCollectionCompatibilityInternal.h"

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace primec::semantics {
namespace {

bool isMapLiteralAssignPair(const Expr &arg) {
  return arg.kind == Expr::Kind::Call && !arg.isMethodCall &&
         !arg.isFieldAccess && arg.name == "assign" &&
         arg.namespacePrefix.empty() && arg.templateArgs.empty() &&
         arg.args.size() == 2;
}

std::vector<const Expr *> collectMapLiteralEntries(const std::vector<Expr> &args) {
  std::vector<const Expr *> entries;
  entries.reserve(args.size() * 2);
  bool allAssignPairs = !args.empty();
  for (const auto &arg : args) {
    if (!isMapLiteralAssignPair(arg)) {
      allAssignPairs = false;
      break;
    }
  }
  const bool flattenAssignPairs = allAssignPairs || (args.size() % 2 != 0);
  for (const auto &arg : args) {
    if (flattenAssignPairs && isMapLiteralAssignPair(arg)) {
      entries.push_back(&arg.args[0]);
      entries.push_back(&arg.args[1]);
    } else {
      entries.push_back(&arg);
    }
  }
  return entries;
}

} // namespace

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
    return failCollectionLiteralDiagnostic(builtinName + " literal does not accept block arguments");
  }
  const std::vector<const Expr *> mapLiteralEntries =
      builtinName == "map" ? collectMapLiteralEntries(expr.args)
                           : std::vector<const Expr *>{};
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
      return failCollectionLiteralDiagnostic("vector literal requires heap_alloc effect");
    }
  }
  if (builtinName == "array" || builtinName == "vector" ||
      builtinName == "soa" "_vector") {
    if (expr.templateArgs.size() != 1) {
      if (builtinName == "array" && expr.templateArgs.size() > 1) {
        return failCollectionLiteralDiagnostic(
            "array<T, N> is unsupported; use array<T> (runtime-count array)");
      }
      return failCollectionLiteralDiagnostic(
          builtinName + " literal requires exactly one template argument");
    }
  } else {
    if (expr.templateArgs.size() != 2) {
      return failCollectionLiteralDiagnostic("map literal requires exactly two template arguments");
    }
    if (mapLiteralEntries.size() % 2 != 0) {
      return failCollectionLiteralDiagnostic("map literal requires an even number of arguments");
    }
  }
  if (builtinName == "map") {
    for (const Expr *arg : mapLiteralEntries) {
      if (!validateExpr(params, locals, *arg)) {
        return false;
      }
    }
  } else {
    for (const auto &arg : expr.args) {
      if (!validateExpr(params, locals, arg)) {
        return false;
      }
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
    const bool isExperimentalVectorContext =
        currentValidationState_.context.definitionPath.rfind(
            experimentalVectorContextPrefix, 0) == 0 ||
        definitionNamespacePrefix.rfind(experimentalVectorContextRoot, 0) == 0;
    if (builtinName == "vector" && !expr.args.empty() &&
        !isExperimentalVectorContext) {
      std::unordered_set<std::string> visitingStructs;
      if (!isRelocationTrivialContainerElementType(
              elemType, definitionNamespacePrefix, definitionTemplateArgs, visitingStructs)) {
        return failCollectionLiteralDiagnostic(
            "vector literal requires relocation-trivial vector element type until container move/reallocation "
            "semantics are implemented: " +
            elemType);
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
    if (!validateBuiltinComparableKeyType(expr.templateArgs.front(), nullptr, error_)) {
      return false;
    }
    const std::vector<std::string> *definitionTemplateArgs = nullptr;
    std::string definitionNamespacePrefix;
    resolveOwnershipContext(definitionNamespacePrefix, definitionTemplateArgs);
    const std::string &keyType = expr.templateArgs[0];
    const std::string &valueType = expr.templateArgs[1];
    if (!mapLiteralEntries.empty()) {
      std::unordered_set<std::string> visitingStructs;
      if (!isRelocationTrivialContainerElementType(
              keyType, definitionNamespacePrefix, definitionTemplateArgs, visitingStructs)) {
        return failCollectionLiteralDiagnostic(
            "map literal requires relocation-trivial map key type until container move/reallocation semantics are "
            "implemented: " +
            keyType);
      }
      visitingStructs.clear();
      if (!isRelocationTrivialContainerElementType(
              valueType, definitionNamespacePrefix, definitionTemplateArgs, visitingStructs)) {
        return failCollectionLiteralDiagnostic(
            "map literal requires relocation-trivial map value type until container move/reallocation semantics are "
            "implemented: " +
            valueType);
      }
    }
    for (size_t i = 0; i + 1 < mapLiteralEntries.size(); i += 2) {
      if (!this->validateCollectionElementType(
              *mapLiteralEntries[i], keyType, "map literal requires key type ", params,
              locals, builtinCollectionDispatchResolvers)) {
        return false;
      }
      if (!this->validateCollectionElementType(
              *mapLiteralEntries[i + 1], valueType,
              "map literal requires value type ", params, locals,
              builtinCollectionDispatchResolvers)) {
        return false;
      }
    }
  }
  return true;
}

} // namespace primec::semantics
