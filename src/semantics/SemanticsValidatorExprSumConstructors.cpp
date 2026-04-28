#include "SemanticsValidator.h"

#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace primec::semantics {
namespace {

bool isEmptyBraceBlockArgument(const Expr &expr) {
  return expr.kind == Expr::Kind::Call && expr.name == "block" &&
         expr.hasBodyArguments && expr.bodyArguments.empty() &&
         expr.args.empty();
}

std::string bindingTypeText(const BindingInfo &binding) {
  if (binding.typeTemplateArg.empty()) {
    return binding.typeName;
  }
  return binding.typeName + "<" + binding.typeTemplateArg + ">";
}

} // namespace

bool SemanticsValidator::isSumDefinition(const Definition &def) const {
  for (const auto &transform : def.transforms) {
    if (transform.name == "sum") {
      return true;
    }
  }
  return false;
}

const Definition *SemanticsValidator::resolveSumDefinitionForTypeText(
    const std::string &typeText,
    const std::string &namespacePrefix) const {
  if (typeText.empty()) {
    return nullptr;
  }
  std::string normalized = normalizeBindingTypeName(typeText);
  std::string base;
  std::string argText;
  if (splitTemplateTypeName(normalized, base, argText)) {
    normalized = normalizeBindingTypeName(base);
  }

  auto definitionForPath = [&](const std::string &path) -> const Definition * {
    if (path.empty() || sumNames_.count(path) == 0) {
      return nullptr;
    }
    auto it = defMap_.find(path);
    if (it == defMap_.end() || it->second == nullptr ||
        !isSumDefinition(*it->second)) {
      return nullptr;
    }
    return it->second;
  };

  if (!normalized.empty() && normalized.front() == '/') {
    return definitionForPath(normalized);
  }

  auto importIt = importAliases_.find(normalized);
  if (importIt != importAliases_.end()) {
    if (const Definition *imported = definitionForPath(importIt->second)) {
      return imported;
    }
  }

  std::string current = namespacePrefix;
  while (true) {
    if (!current.empty()) {
      const std::string direct = current + "/" + normalized;
      if (const Definition *directDef = definitionForPath(direct)) {
        return directDef;
      }
      if (current.size() > normalized.size()) {
        const size_t start = current.size() - normalized.size();
        if (start > 0 && current[start - 1] == '/' &&
            current.compare(start, normalized.size(), normalized) == 0) {
          if (const Definition *currentDef = definitionForPath(current)) {
            return currentDef;
          }
        }
      }
    } else {
      if (const Definition *rootDef = definitionForPath("/" + normalized)) {
        return rootDef;
      }
    }
    if (current.empty()) {
      break;
    }
    const size_t slash = current.find_last_of('/');
    if (slash == std::string::npos || slash == 0) {
      current.clear();
    } else {
      current.erase(slash);
    }
  }

  const std::string suffix = "/" + normalized;
  const Definition *uniqueMatch = nullptr;
  for (const auto &path : sumNames_) {
    if (path.size() < suffix.size() ||
        path.compare(path.size() - suffix.size(), suffix.size(), suffix) != 0) {
      continue;
    }
    const Definition *candidate = definitionForPath(path);
    if (candidate == nullptr) {
      continue;
    }
    if (uniqueMatch != nullptr && uniqueMatch != candidate) {
      return nullptr;
    }
    uniqueMatch = candidate;
  }
  return uniqueMatch;
}

bool SemanticsValidator::inferExplicitSumConstructorBinding(
    const Expr &initializer,
    BindingInfo &bindingOut) const {
  if (initializer.kind != Expr::Kind::Call || initializer.isMethodCall ||
      initializer.isFieldAccess || !initializer.isBraceConstructor) {
    return false;
  }
  const Definition *sumDef =
      resolveSumDefinitionForTypeText(initializer.name,
                                      initializer.namespacePrefix);
  if (sumDef == nullptr) {
    return false;
  }

  const std::vector<Expr> *constructorArgs = &initializer.args;
  const std::vector<std::optional<std::string>> *constructorArgNames =
      &initializer.argNames;
  std::vector<Expr> normalizedArgs;
  std::vector<std::optional<std::string>> normalizedArgNames;
  if (initializer.args.size() == 1 && initializer.argNames.size() == 1 &&
      !initializer.argNames.front().has_value() &&
      isEmptyBraceBlockArgument(initializer.args.front())) {
    constructorArgs = &normalizedArgs;
    constructorArgNames = &normalizedArgNames;
  }
  if (constructorArgs->size() != 1 || constructorArgNames->size() != 1 ||
      !constructorArgNames->front().has_value()) {
    return false;
  }
  const std::string &variantName = *constructorArgNames->front();
  for (const auto &variant : sumDef->sumVariants) {
    if (variant.name == variantName) {
      bindingOut.typeName = sumDef->fullPath;
      bindingOut.typeTemplateArg.clear();
      return true;
    }
  }
  return false;
}

bool SemanticsValidator::validateExplicitSumConstructorExpr(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &expr,
    bool &handledOut) {
  handledOut = false;
  if (expr.kind != Expr::Kind::Call || expr.isMethodCall || expr.isFieldAccess) {
    return true;
  }
  const Definition *sumDef =
      resolveSumDefinitionForTypeText(expr.name, expr.namespacePrefix);
  if (sumDef == nullptr) {
    return true;
  }
  handledOut = true;
  auto failSumConstructorDiagnostic = [&](std::string message) -> bool {
    return failExprDiagnostic(expr, std::move(message));
  };
  if (!expr.isBraceConstructor) {
    return failSumConstructorDiagnostic(
        "sum construction requires braces: " + sumDef->fullPath);
  }
  if (!expr.templateArgs.empty() || expr.hasBodyArguments ||
      !expr.bodyArguments.empty()) {
    return failSumConstructorDiagnostic(
        "sum construction requires exactly one explicit variant for " +
        sumDef->fullPath);
  }

  const std::vector<Expr> *constructorArgs = &expr.args;
  const std::vector<std::optional<std::string>> *constructorArgNames =
      &expr.argNames;
  std::vector<Expr> normalizedArgs;
  std::vector<std::optional<std::string>> normalizedArgNames;
  if (expr.args.size() == 1 && expr.argNames.size() == 1 &&
      !expr.argNames.front().has_value() &&
      isEmptyBraceBlockArgument(expr.args.front())) {
    constructorArgs = &normalizedArgs;
    constructorArgNames = &normalizedArgNames;
  }

  std::unordered_set<std::string> seenVariants;
  for (const auto &argName : *constructorArgNames) {
    if (!argName.has_value()) {
      continue;
    }
    if (!seenVariants.insert(*argName).second) {
      return failSumConstructorDiagnostic(
          "duplicate sum variant in construction: " + *argName +
          " on " + sumDef->fullPath);
    }
  }
  if (constructorArgs->size() != 1 || constructorArgNames->size() != 1) {
    return failSumConstructorDiagnostic(
        "sum construction requires exactly one explicit variant for " +
        sumDef->fullPath);
  }
  if (!constructorArgNames->front().has_value()) {
    return failSumConstructorDiagnostic(
        "sum construction requires explicit variant label for " +
        sumDef->fullPath);
  }

  const std::string &variantName = *constructorArgNames->front();
  const SumVariant *selectedVariant = nullptr;
  for (const auto &variant : sumDef->sumVariants) {
    if (variant.name == variantName) {
      selectedVariant = &variant;
      break;
    }
  }
  if (selectedVariant == nullptr) {
    return failSumConstructorDiagnostic(
        "unknown sum variant on " + sumDef->fullPath + ": " + variantName);
  }

  const Expr &payloadExpr = constructorArgs->front();
  if (!validateExpr(params, locals, payloadExpr)) {
    return false;
  }

  ParameterInfo payloadParam;
  payloadParam.name = selectedVariant->name;
  payloadParam.binding.typeName = selectedVariant->payloadType;
  if (!selectedVariant->payloadTemplateArgs.empty()) {
    payloadParam.binding.typeTemplateArg =
        joinTemplateArgs(selectedVariant->payloadTemplateArgs);
  }

  const BuiltinCollectionDispatchResolverAdapters dispatchResolverAdapters;
  const BuiltinCollectionDispatchResolvers dispatchResolvers =
      makeBuiltinCollectionDispatchResolvers(params,
                                             locals,
                                             dispatchResolverAdapters);
  const std::string resolved = sumDef->fullPath;
  const std::string diagnosticResolved = sumDef->fullPath;
  ExprArgumentValidationContext argumentContext;
  argumentContext.callExpr = &expr;
  argumentContext.resolved = &resolved;
  argumentContext.diagnosticResolved = &diagnosticResolved;
  argumentContext.params = &params;
  argumentContext.locals = &locals;
  argumentContext.dispatchResolvers = &dispatchResolvers;
  return validateArgumentTypeAgainstParam(payloadExpr,
                                          payloadParam,
                                          payloadParam.binding.typeName,
                                          bindingTypeText(payloadParam.binding),
                                          argumentContext);
}

} // namespace primec::semantics
