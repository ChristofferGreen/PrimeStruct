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

using semantics::isPickCall;
using semantics::canonicalizeLegacySoaToAosHelperPath;
using semantics::isVectorCompatibilityHelperName;
using semantics::isExperimentalSoaGetLikeHelperPath;
using semantics::isExperimentalSoaRefLikeHelperPath;
using semantics::isPublishedVectorMutatorHelperName;

using semantics::isBindingAuxTransformName;
using semantics::isRootBuiltinName;
using semantics::isCanonicalVectorCompatibilityPath;
using semantics::getBuiltinArrayAccessName;
using semantics::isExperimentalSoaVectorTypePath;
using semantics::soaUnavailableMethodDiagnostic;
using semantics::trimLeadingSlash;

using semantics::canonicalizeLegacySoaRefHelperPath;
using semantics::isCompileTimeTypeBinding;
using semantics::isExperimentalSoaVectorHelperFamilyPath;
using semantics::isKeyValueCollectionTypeName;
using semantics::isLegacyExperimentalVectorCompatibilityPath;
using semantics::isLegacyExperimentalVectorCompatibilitySpecializedTypePath;
using semantics::legacyExperimentalVectorCompatibilityPrefix;
using semantics::preferredPublishedCollectionLoweringPath;
using semantics::resolveCanonicalVectorHelperNameFromResolvedPath;
using semantics::resolveVectorCompatibilityHelperNameFromResolvedPath;
using semantics::vectorHelperSurfaceMetadata;

using semantics::hasNamedArguments;
using semantics::getBuiltinPointerName;
using semantics::vectorConstructorSurfaceMetadata;
using semantics::canonicalVectorCompatibilityPrefixOrFallback;

using semantics::joinTemplateArgs;
using semantics::canonicalVectorTypeIdentityPrefix;
using semantics::legacyExperimentalVectorCompatibilityTypeText;
using semantics::returnKindForTypeName;
using semantics::resolveTypePath;
using semantics::mapCollectionAliasToken;
using semantics::stripUnrootedCanonicalVectorCompatibilityPrefix;
using semantics::publicSoaHelperTargetPath;
using semantics::isUnrootedCanonicalVectorCompatibilityPath;
using semantics::isSoftwareNumericTypeName;
using semantics::isLegacyOrCanonicalSoaHelperPath;
using semantics::isIfCall;
using semantics::isCanonicalSoaRefLikeHelperPath;
using semantics::compatibilitySoaHelperTargetPath;
using semantics::canonicalizeLegacySoaGetHelperPath;
using semantics::canonicalVectorCompatibilityHelperPathOrFallback;

using semantics::BindingInfo;
using semantics::ParameterInfo;
using semantics::ReturnKind;
using semantics::buildOrderedArguments;
using semantics::extractKeyValueCollectionTypesFromTypeText;
using semantics::getBuiltinCollectionName;
using semantics::isExperimentalSoaVectorSpecializedTypePath;
using semantics::isPrimitiveBindingTypeName;
using semantics::isReturnCall;
using semantics::isSimpleCallName;
using semantics::hasExplicitBindingTypeTransform;
using semantics::normalizeBindingTypeName;
using semantics::splitTemplateTypeName;
using semantics::splitTopLevelTemplateArgs;



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
  if (splitTemplateTypeName(typeText, base, argText) && normalizeBindingTypeName(base) == "uninitialized") {
    std::vector<std::string> storageArgs;
    if (!splitTopLevelTemplateArgs(argText, storageArgs) || storageArgs.size() != 1) {
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
  if (!splitTemplateTypeName(typeText, base, argText) || normalizeBindingTypeName(base) != "Result") {
    return true;
  }
  std::vector<std::string> resultArgs;
  if (!splitTopLevelTemplateArgs(argText, resultArgs) || resultArgs.size() != 2) {
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
  if (splitTemplateTypeName(typeText, base, argText) && normalizeBindingTypeName(base) == "uninitialized") {
    std::vector<std::string> storageArgs;
    if (!splitTopLevelTemplateArgs(argText, storageArgs) || storageArgs.size() != 1) {
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
                                           const std::vector<ParameterInfo> &params,
                                           const LocalTypeMap &locals,
                                           bool allowMathBare,
                                           Context &ctx,
                                           ExpectedTypeFn &&hasExpectedType,
                                           std::string_view compatibilityEnvelope,
                                           RewriteTargetValueFn &&rewriteTargetValue) {
  if (!bindingExpr.isBinding || bindingExpr.args.size() != 1) {
    return true;
  }
  BindingInfo bindingInfo;
  const bool hasExplicitBindingTransform = hasExplicitBindingTypeTransform(bindingExpr);
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
