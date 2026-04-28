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

std::string variantNamesList(const std::vector<const SumVariant *> &variants) {
  std::string out;
  for (const SumVariant *variant : variants) {
    if (variant == nullptr) {
      continue;
    }
    if (!out.empty()) {
      out += ", ";
    }
    out += variant->name;
  }
  return out;
}

std::string allVariantNamesList(const Definition &sumDef) {
  std::string out;
  for (const auto &variant : sumDef.sumVariants) {
    if (!out.empty()) {
      out += ", ";
    }
    out += variant.name;
  }
  return out;
}

const SumVariant *findVariantByName(const Definition &sumDef,
                                    const std::string &variantName) {
  for (const auto &variant : sumDef.sumVariants) {
    if (variant.name == variantName) {
      return &variant;
    }
  }
  return nullptr;
}

const SumVariant *defaultUnitVariant(const Definition &sumDef) {
  if (sumDef.sumVariants.empty() || sumDef.sumVariants.front().hasPayload) {
    return nullptr;
  }
  return &sumDef.sumVariants.front();
}

const SumVariant *unitVariantForNameInitializer(const Definition &sumDef,
                                                const Expr &initializer) {
  if (initializer.kind != Expr::Kind::Name) {
    return nullptr;
  }
  const SumVariant *variant = findVariantByName(sumDef, initializer.name);
  if (variant == nullptr || variant->hasPayload) {
    return nullptr;
  }
  return variant;
}

const SumVariant *unitVariantForConstructorArgument(const Definition &sumDef,
                                                    const Expr &arg) {
  if (const SumVariant *variant = unitVariantForNameInitializer(sumDef, arg)) {
    return variant;
  }
  if (arg.kind != Expr::Kind::Call || arg.name != "block" ||
      !arg.hasBodyArguments || !arg.args.empty() ||
      arg.bodyArguments.size() != 1) {
    return nullptr;
  }
  return unitVariantForNameInitializer(sumDef, arg.bodyArguments.front());
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
    if (constructorArgs->empty() && constructorArgNames->empty()) {
      if (defaultUnitVariant(*sumDef) == nullptr) {
        return false;
      }
      bindingOut.typeName = sumDef->fullPath;
      bindingOut.typeTemplateArg.clear();
      return true;
    }
    if (constructorArgs->size() == 1 && constructorArgNames->size() == 1 &&
        !constructorArgNames->front().has_value() &&
        unitVariantForConstructorArgument(*sumDef, constructorArgs->front()) != nullptr) {
      bindingOut.typeName = sumDef->fullPath;
      bindingOut.typeTemplateArg.clear();
      return true;
    }
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

const Definition *SemanticsValidator::resolvePickSumDefinitionForExpr(
    const Expr &expr,
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const std::string &namespacePrefix) {
  auto resolveBinding = [&](const BindingInfo &binding,
                            const std::string &bindingNamespace) {
    return resolveSumDefinitionForTypeText(bindingTypeText(binding),
                                           bindingNamespace);
  };
  if (expr.kind == Expr::Kind::Name) {
    if (const BindingInfo *paramBinding = findParamBinding(params, expr.name)) {
      if (const Definition *sumDef = resolveBinding(*paramBinding,
                                                    namespacePrefix)) {
        return sumDef;
      }
    }
    auto localIt = locals.find(expr.name);
    if (localIt != locals.end()) {
      if (const Definition *sumDef = resolveBinding(localIt->second,
                                                    namespacePrefix)) {
        return sumDef;
      }
    }
  }

  BindingInfo inferredBinding;
  if (inferExplicitSumConstructorBinding(expr, inferredBinding) ||
      inferBindingTypeFromInitializer(expr, params, locals, inferredBinding)) {
    if (const Definition *sumDef =
            resolveBinding(inferredBinding, expr.namespacePrefix)) {
      return sumDef;
    }
    if (const Definition *sumDef =
            resolveBinding(inferredBinding, namespacePrefix)) {
      return sumDef;
    }
  }

  std::string inferredTypeText;
  if (inferQueryExprTypeText(expr, params, locals, inferredTypeText) &&
      !inferredTypeText.empty()) {
    if (const Definition *sumDef =
            resolveSumDefinitionForTypeText(inferredTypeText,
                                            expr.namespacePrefix)) {
      return sumDef;
    }
    return resolveSumDefinitionForTypeText(inferredTypeText, namespacePrefix);
  }
  return nullptr;
}

bool SemanticsValidator::validateTargetTypedSumInitializer(
    const std::string &targetTypeText,
    const Expr &initializer,
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const std::string &namespacePrefix,
    bool &handledOut) {
  handledOut = false;
  const Definition *sumDef =
      resolveSumDefinitionForTypeText(targetTypeText, namespacePrefix);
  if (sumDef == nullptr) {
    return true;
  }
  handledOut = true;

  auto failInferredSumDiagnostic = [&](std::string message) -> bool {
    return failExprDiagnostic(initializer, std::move(message));
  };

  BindingInfo explicitSumBinding;
  if (inferExplicitSumConstructorBinding(initializer, explicitSumBinding)) {
    if (explicitSumBinding.typeName == sumDef->fullPath) {
      return validateExpr(params, locals, initializer);
    }
    return failInferredSumDiagnostic(
        "sum construction target mismatch for " + sumDef->fullPath +
        ": got " + explicitSumBinding.typeName);
  }
  if (initializer.argNames.size() == 1 && initializer.argNames.front().has_value()) {
    const std::string &variantName = *initializer.argNames.front();
    for (const auto &variant : sumDef->sumVariants) {
      if (variant.name == variantName && !variant.hasPayload) {
        return failInferredSumDiagnostic(
            "unit sum variant does not accept payload: " +
            sumDef->fullPath + "/" + variant.name);
      }
    }
  }
  if (isEmptyBraceBlockArgument(initializer)) {
    if (defaultUnitVariant(*sumDef) != nullptr) {
      return true;
    }
    return failInferredSumDiagnostic(
        "default sum construction requires first variant to be unit: " +
        sumDef->fullPath);
  }
  if (unitVariantForNameInitializer(*sumDef, initializer) != nullptr) {
    return true;
  }
  auto resolvesToTargetSum = [&](const std::string &typeText,
                                 const std::string &typeNamespace) -> bool {
    const Definition *actualSum =
        resolveSumDefinitionForTypeText(typeText, typeNamespace);
    return actualSum != nullptr && actualSum->fullPath == sumDef->fullPath;
  };
  BindingInfo directBinding;
  if (inferBindingTypeFromInitializer(initializer,
                                      params,
                                      locals,
                                      directBinding) &&
      resolvesToTargetSum(bindingTypeText(directBinding),
                          initializer.namespacePrefix)) {
    return validateExpr(params, locals, initializer);
  }
  std::string directTypeText;
  if (inferQueryExprTypeText(initializer, params, locals, directTypeText) &&
      resolvesToTargetSum(directTypeText, initializer.namespacePrefix)) {
    return validateExpr(params, locals, initializer);
  }

  auto payloadTypeText = [](const SumVariant &variant) {
    if (!variant.payloadTypeText.empty()) {
      return variant.payloadTypeText;
    }
    if (variant.payloadTemplateArgs.empty()) {
      return variant.payloadType;
    }
    return variant.payloadType + "<" +
           joinTemplateArgs(variant.payloadTemplateArgs) + ">";
  };

  auto bindingMatchesExpected =
      [&](const BindingInfo &actualBinding,
          const std::string &expectedTypeText,
          const std::string &expectedNamespace) -> bool {
    const std::string actualTypeText = bindingTypeText(actualBinding);
    if (actualTypeText.empty()) {
      return false;
    }
    if (errorTypesMatch(expectedTypeText, actualTypeText, expectedNamespace)) {
      return true;
    }
    if (const Definition *expectedSum =
            resolveSumDefinitionForTypeText(expectedTypeText,
                                            expectedNamespace)) {
      if (const Definition *actualSum =
              resolveSumDefinitionForTypeText(actualTypeText,
                                              initializer.namespacePrefix)) {
        return actualSum->fullPath == expectedSum->fullPath;
      }
      if (const Definition *actualSum =
              resolveSumDefinitionForTypeText(actualTypeText,
                                              expectedNamespace)) {
        return actualSum->fullPath == expectedSum->fullPath;
      }
      return actualBinding.typeName == expectedSum->fullPath;
    }

    const std::string expectedStructPath =
        resolveStructTypePath(expectedTypeText, expectedNamespace,
                              structNames_);
    if (!expectedStructPath.empty()) {
      std::string actualStructPath =
          resolveStructTypePath(actualTypeText, initializer.namespacePrefix,
                                structNames_);
      if (actualStructPath.empty()) {
        actualStructPath = resolveStructTypePath(actualBinding.typeName,
                                                initializer.namespacePrefix,
                                                structNames_);
      }
      if (actualStructPath.empty()) {
        actualStructPath = resolveStructTypePath(actualTypeText,
                                                expectedNamespace,
                                                structNames_);
      }
      return actualStructPath == expectedStructPath;
    }

    const ReturnKind expectedKind =
        returnKindForTypeName(normalizeBindingTypeName(expectedTypeText));
    if (expectedKind == ReturnKind::Unknown) {
      return false;
    }
    const ReturnKind actualKind = returnKindForTypeName(
        normalizeBindingTypeName(actualBinding.typeName));
    return actualKind == expectedKind;
  };

  auto payloadMatchesVariant = [&](const SumVariant &variant) -> bool {
    if (!variant.hasPayload) {
      return false;
    }
    const std::string expectedTypeText = payloadTypeText(variant);
    BindingInfo inferredBinding;
    if (inferExplicitSumConstructorBinding(initializer, inferredBinding) ||
        inferBindingTypeFromInitializer(initializer,
                                        params,
                                        locals,
                                        inferredBinding)) {
      if (bindingMatchesExpected(inferredBinding,
                                 expectedTypeText,
                                 sumDef->namespacePrefix)) {
        return true;
      }
    }

    const std::string expectedStructPath =
        resolveStructTypePath(expectedTypeText, sumDef->namespacePrefix,
                              structNames_);
    if (!expectedStructPath.empty()) {
      const std::string actualStructPath =
          inferStructReturnPath(initializer, params, locals);
      if (!actualStructPath.empty()) {
        return actualStructPath == expectedStructPath;
      }
    }

    std::string inferredTypeText;
    if (inferQueryExprTypeText(initializer,
                               params,
                               locals,
                               inferredTypeText) &&
        !inferredTypeText.empty()) {
      BindingInfo inferredTypeBinding;
      std::string base;
      std::string argText;
      if (splitTemplateTypeName(inferredTypeText, base, argText)) {
        inferredTypeBinding.typeName = normalizeBindingTypeName(base);
        inferredTypeBinding.typeTemplateArg = argText;
      } else {
        inferredTypeBinding.typeName = normalizeBindingTypeName(inferredTypeText);
      }
      if (bindingMatchesExpected(inferredTypeBinding,
                                 expectedTypeText,
                                 sumDef->namespacePrefix)) {
        return true;
      }
    }

    const ReturnKind expectedKind =
        returnKindForTypeName(normalizeBindingTypeName(expectedTypeText));
    if (expectedKind == ReturnKind::Unknown) {
      return false;
    }
    const ReturnKind actualKind = inferExprReturnKind(initializer,
                                                      params,
                                                      locals);
    return actualKind == expectedKind;
  };

  std::vector<const SumVariant *> matches;
  for (const auto &variant : sumDef->sumVariants) {
    if (payloadMatchesVariant(variant)) {
      matches.push_back(&variant);
    }
  }

  if (matches.empty()) {
    return failInferredSumDiagnostic(
        "no inferred sum variant for " + sumDef->fullPath +
        " accepts payload; candidates: " + allVariantNamesList(*sumDef));
  }
  if (matches.size() > 1) {
    return failInferredSumDiagnostic(
        "ambiguous inferred sum construction for " + sumDef->fullPath +
        ": " + variantNamesList(matches));
  }

  if (!validateExpr(params, locals, initializer)) {
    return false;
  }

  const SumVariant &selectedVariant = *matches.front();
  ParameterInfo payloadParam;
  payloadParam.name = selectedVariant.name;
  payloadParam.binding.typeName = selectedVariant.payloadType;
  if (!selectedVariant.payloadTemplateArgs.empty()) {
    payloadParam.binding.typeTemplateArg =
        joinTemplateArgs(selectedVariant.payloadTemplateArgs);
  }

  const BuiltinCollectionDispatchResolverAdapters dispatchResolverAdapters;
  const BuiltinCollectionDispatchResolvers dispatchResolvers =
      makeBuiltinCollectionDispatchResolvers(params,
                                             locals,
                                             dispatchResolverAdapters);
  const std::string resolved = sumDef->fullPath;
  const std::string diagnosticResolved = sumDef->fullPath;
  Expr contextExpr = initializer;
  contextExpr.namespacePrefix = sumDef->namespacePrefix;
  ExprArgumentValidationContext argumentContext;
  argumentContext.callExpr = &contextExpr;
  argumentContext.resolved = &resolved;
  argumentContext.diagnosticResolved = &diagnosticResolved;
  argumentContext.params = &params;
  argumentContext.locals = &locals;
  argumentContext.dispatchResolvers = &dispatchResolvers;
  return validateArgumentTypeAgainstParam(
      initializer,
      payloadParam,
      payloadParam.binding.typeName,
      bindingTypeText(payloadParam.binding),
      argumentContext);
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
  if (constructorArgs->empty() && constructorArgNames->empty()) {
    if (defaultUnitVariant(*sumDef) != nullptr) {
      return true;
    }
    return failSumConstructorDiagnostic(
        "default sum construction requires first variant to be unit: " +
        sumDef->fullPath);
  }
  if (constructorArgs->size() == 1 && constructorArgNames->size() == 1 &&
      !constructorArgNames->front().has_value()) {
    if (unitVariantForConstructorArgument(*sumDef, constructorArgs->front()) != nullptr) {
      return true;
    }
    return failSumConstructorDiagnostic(
        "sum construction requires explicit variant label for " +
        sumDef->fullPath);
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
  if (!selectedVariant->hasPayload) {
    return failSumConstructorDiagnostic(
        "unit sum variant does not accept payload: " +
        sumDef->fullPath + "/" + selectedVariant->name);
  }
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
