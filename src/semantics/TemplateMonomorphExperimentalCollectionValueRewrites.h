#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "TemplateMonomorphContext.h"
#include "primec/ast/Ast.h"
#include "SemanticsHelpers.h"
#include "primec/support/StdlibSurfaceRegistry.h"
#include "SemanticsValidatorInferCollectionCompatibilityInternal.h"
#include "TemplateMonomorphExperimentalCollectionReturnSetup.h"
#include "TemplateMonomorphCoreUtilities.h"
#include "TemplateMonomorphFallbackTypeInference.h"
#include "TemplateMonomorphExperimentalCollectionTypeHelpers.h"
#include "TemplateMonomorphSetupUtilities.h"
#include "TemplateMonomorphCollectionCompatibilityPaths.h"
#include "TemplateMonomorphImplicitTemplateInference.h"

namespace primec {

bool isBuiltinResultOkPayloadCall(const Expr &candidate);

template <typename RewriteCurrentFn>
bool rewriteExperimentalConstructorValueTree(Expr &candidate, RewriteCurrentFn &&rewriteCurrent) {
  if (candidate.isBinding && candidate.args.size() == 1) {
    return rewriteExperimentalConstructorValueTree(candidate.args.front(), rewriteCurrent);
  }
  if (candidate.kind != Expr::Kind::Call) {
    return true;
  }
  if (!rewriteCurrent(candidate)) {
    return false;
  }
  for (auto &arg : candidate.args) {
    if (!rewriteExperimentalConstructorValueTree(arg, rewriteCurrent)) {
      return false;
    }
  }
  for (auto &bodyArg : candidate.bodyArguments) {
    if (!rewriteExperimentalConstructorValueTree(bodyArg, rewriteCurrent)) {
      return false;
    }
  }
  return true;
}

template <typename RewriteKeyValueValueFn>
bool rewriteExperimentalKeyValueResultOkPayloadTree(Expr &candidate, RewriteKeyValueValueFn &&rewriteKeyValueValue) {
  if (candidate.isBinding && candidate.args.size() == 1) {
    return rewriteExperimentalKeyValueResultOkPayloadTree(candidate.args.front(), rewriteKeyValueValue);
  }
  if (candidate.kind != Expr::Kind::Call) {
    return true;
  }
  if (isBuiltinResultOkPayloadCall(candidate)) {
    return rewriteKeyValueValue(candidate.args.back());
  }
  for (auto &arg : candidate.args) {
    if (!rewriteExperimentalKeyValueResultOkPayloadTree(arg, rewriteKeyValueValue)) {
      return false;
    }
  }
  for (auto &bodyArg : candidate.bodyArguments) {
    if (!rewriteExperimentalKeyValueResultOkPayloadTree(bodyArg, rewriteKeyValueValue)) {
      return false;
    }
  }
  return true;
}

template <typename RewriteNestedKeyValueValueFn, typename RewriteKeyValuePayloadFn>
bool rewriteExperimentalKeyValueTargetValueForType(const std::string &typeText,
                                                   Expr &valueExpr,
                                                   const SubstMap &mapping,
                                                   const std::unordered_set<std::string> &allowedParams,
                                                   const std::string &namespacePrefix,
                                                   Context &ctx,
                                                   RewriteNestedKeyValueValueFn &&rewriteNestedKeyValueValue,
                                                   RewriteKeyValuePayloadFn &&rewriteKeyValuePayload) {
  std::string base;
  std::string argText;
  if (semantics::splitTemplateTypeName(typeText, base, argText) && semantics::normalizeBindingTypeName(base) == "uninitialized") {
    std::vector<std::string> storageArgs;
    if (!semantics::splitTopLevelTemplateArgs(argText, storageArgs) || storageArgs.size() != 1) {
      return true;
    }
    return rewriteExperimentalKeyValueTargetValueForType(trimWhitespace(storageArgs.front()),
                                                         valueExpr,
                                                         mapping,
                                                         allowedParams,
                                                         namespacePrefix,
                                                         ctx,
                                                         rewriteNestedKeyValueValue,
                                                         rewriteKeyValuePayload);
  }
  if (resolvesExperimentalKeyValueTypeText(typeText, mapping, allowedParams, namespacePrefix, ctx)) {
    return rewriteNestedKeyValueValue(valueExpr);
  }
  if (!semantics::splitTemplateTypeName(typeText, base, argText) || semantics::normalizeBindingTypeName(base) != "Result") {
    return true;
  }
  std::vector<std::string> resultArgs;
  if (!semantics::splitTopLevelTemplateArgs(argText, resultArgs) || resultArgs.size() != 2) {
    return true;
  }
  if (!resolvesExperimentalKeyValueTypeText(trimWhitespace(resultArgs.front()),
                                            mapping,
                                            allowedParams,
                                            namespacePrefix,
                                            ctx)) {
    return true;
  }
  return rewriteKeyValuePayload(valueExpr);
}

template <typename RewriteNestedVectorValueFn>
bool rewriteExperimentalVectorTargetValueForType(const std::string &typeText,
                                                 Expr &valueExpr,
                                                 RewriteNestedVectorValueFn &&rewriteNestedVectorValue) {
  std::string base;
  std::string argText;
  if (semantics::splitTemplateTypeName(typeText, base, argText) && semantics::normalizeBindingTypeName(base) == "uninitialized") {
    std::vector<std::string> storageArgs;
    if (!semantics::splitTopLevelTemplateArgs(argText, storageArgs) || storageArgs.size() != 1) {
      return true;
    }
    return rewriteExperimentalVectorTargetValueForType(trimWhitespace(storageArgs.front()),
                                                       valueExpr,
                                                       rewriteNestedVectorValue);
  }
  if (!resolvesCollectionVectorValueTypeText(typeText)) {
    return true;
  }
  return rewriteNestedVectorValue(valueExpr);
}

template <typename ExpectedTypeFn, typename RewriteTargetValueFn>
bool rewriteExperimentalConstructorBinding(Expr &bindingExpr,
                                           const std::vector<semantics::ParameterInfo> &params,
                                           const LocalTypeMap &locals,
                                           bool allowMathBare,
                                           Context &ctx,
                                           ExpectedTypeFn &&hasExpectedType,
                                           std::string_view compatibilityEnvelope,
                                           RewriteTargetValueFn &&rewriteTargetValue) {
  if (!bindingExpr.isBinding || bindingExpr.args.size() != 1) {
    return true;
  }
  semantics::BindingInfo bindingInfo;
  const bool hasExplicitBindingTransform = semantics::hasExplicitBindingTypeTransform(bindingExpr);
  const bool hasExplicitBindingType = extractExplicitBindingType(bindingExpr, bindingInfo);
  if (hasExplicitBindingType) {
    const std::string bindingTypeText = bindingTypeToString(bindingInfo);
    if (!hasExpectedType(bindingTypeText) &&
        unwrapCollectionReceiverEnvelope(bindingInfo.typeName, bindingInfo.typeTemplateArg) ==
            compatibilityEnvelope) {
      return true;
    }
  }
  if (!hasExplicitBindingType) {
    if (hasExplicitBindingTransform) {
      return true;
    }
    if (!inferBindingTypeForMonomorph(bindingExpr.args.front(), params, locals, allowMathBare, ctx, bindingInfo)) {
      return true;
    }
  } else if (bindingInfo.typeName == "auto") {
    if (!inferBindingTypeForMonomorph(bindingExpr.args.front(), params, locals, allowMathBare, ctx, bindingInfo)) {
      return true;
    }
  }
  std::string bindingTypeText = bindingInfo.typeName;
  if (!bindingInfo.typeTemplateArg.empty()) {
    bindingTypeText += "<" + bindingInfo.typeTemplateArg + ">";
  }
  return rewriteTargetValue(bindingTypeText, bindingExpr.args.front());
}

} // namespace primec
