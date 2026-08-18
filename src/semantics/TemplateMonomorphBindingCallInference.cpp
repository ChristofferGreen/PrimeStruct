#include "TemplateMonomorphAssignmentTargetResolution.h"
#include "TemplateMonomorphBindingBlockInference.h"
#include "TemplateMonomorphBindingCallInference.h"
#include "TemplateMonomorphDefinitionBindingSetup.h"
#include "TemplateMonomorphDefinitionExperimentalCollectionRewrites.h"
#include "TemplateMonomorphDefinitionReturnOrchestration.h"
#include "TemplateMonomorphDefinitionRewrites.h"
#include "TemplateMonomorphExecutionRewrites.h"
#include "TemplateMonomorphExperimentalCollectionArgumentRewrites.h"
#include "TemplateMonomorphExperimentalCollectionConstructorRewrites.h"
#include "TemplateMonomorphExperimentalCollectionReceiverResolution.h"
#include "TemplateMonomorphExperimentalCollectionReturnRewrites.h"
#include "TemplateMonomorphExperimentalCollectionReturnSetup.h"
#include "TemplateMonomorphExperimentalCollectionTargetValueRewrites.h"
#include "TemplateMonomorphExperimentalCollectionValueRewrites.h"
#include "TemplateMonomorphExpressionRewrite.h"
#include "TemplateMonomorphFallbackTypeInference.h"
#include "TemplateMonomorphFinalOrchestration.h"
#include "TemplateMonomorphImplicitTemplateInference.h"
#include "TemplateMonomorphMethodTargets.h"
#include "TemplateMonomorphTemplateSpecialization.h"
#include "TemplateMonomorphTypeResolution.h"
#include "SemanticsHelpers.h"
#include "SemanticsValidatorInferCollectionCompatibilityInternal.h"
#include "StdlibCollectionSurfaceHelpers.h"
#include "TemplateMonomorphCoreUtilities.h"
#include "TemplateMonomorphSetupUtilities.h"
#include "TemplateMonomorphCollectionCompatibilityPaths.h"
#include "TemplateMonomorphExperimentalCollectionTypeHelpers.h"
#include "TemplateMonomorphSourceDefinitionSetup.h"
#include "TemplateMonomorphExperimentalCollectionConstructorPaths.h"
#include "primec/support/CollectionSpellingClassifier.h"
#include "primec/support/StdlibSurfaceRegistry.h"

#include <sstream>

#include "primec/support/CompileArena.h"

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
using semantics::normalizeBindingTypeName;
using semantics::splitTemplateTypeName;
using semantics::splitTopLevelTemplateArgs;


bool inferCallBindingTypeForMonomorph(const Expr &initializer,
                                      const std::vector<ParameterInfo> &params,
                                      const LocalTypeMap &locals,
                                      bool allowMathBare,
                                      Context &ctx,
                                      BindingInfo &infoOut,
                                      bool &handledOut) {
  handledOut = false;
  if (initializer.kind != Expr::Kind::Call) {
    return false;
  }

  if (initializer.isMethodCall && initializer.name == "ok" && initializer.args.size() == 2 &&
      initializer.templateArgs.empty() && !initializer.hasBodyArguments && initializer.bodyArguments.empty()) {
    const Expr &receiver = initializer.args.front();
    if (receiver.kind == Expr::Kind::Name && normalizeBindingTypeName(receiver.name) == "Result") {
      BindingInfo payloadInfo;
      if (!inferBindingTypeForMonomorph(initializer.args.back(), params, locals, allowMathBare, ctx, payloadInfo)) {
        handledOut = true;
        return false;
      }
      const std::string payloadTypeText = bindingTypeToString(payloadInfo);
      if (payloadTypeText.empty()) {
        handledOut = true;
        return false;
      }
      infoOut.typeName = "Result";
      infoOut.typeTemplateArg = payloadTypeText + ", _";
      return true;
    }
  }
  if (isIfCall(initializer) && initializer.args.size() == 3) {
    BindingInfo thenInfo;
    BindingInfo elseInfo;
    if (!inferBindingTypeForMonomorph(initializer.args[1], params, locals, allowMathBare, ctx, thenInfo) ||
        !inferBindingTypeForMonomorph(initializer.args[2], params, locals, allowMathBare, ctx, elseInfo)) {
      handledOut = true;
      return false;
    }
    if (bindingTypeToString(thenInfo) != bindingTypeToString(elseInfo)) {
      handledOut = true;
      return false;
    }
    infoOut = thenInfo;
    return true;
  }

  std::string resolved;
  if (initializer.isMethodCall) {
    if (!resolveMethodCallTemplateTarget(initializer, locals, ctx, resolved)) {
      resolved.clear();
    }
  } else {
    resolved = resolveCalleePath(initializer, initializer.namespacePrefix, ctx, &locals, &params);
  }

  if (!initializer.isMethodCall && initializer.templateArgs.size() == 1) {
    const std::string experimentalPath = experimentalVectorConstructorInferencePath(resolved);
    if (!experimentalPath.empty() && ctx.sourceDefs.count(experimentalPath) > 0) {
      handledOut = true;
      infoOut.typeName = canonicalVectorTypeIdentityPrefix() + "Vector";
      infoOut.typeTemplateArg = joinTemplateArgs(initializer.templateArgs);
      return true;
    }
    if (isCollectionVectorConstructorHelperPath(resolved)) {
      handledOut = true;
      infoOut.typeName = canonicalVectorTypeIdentityPrefix() + "Vector";
      infoOut.typeTemplateArg = joinTemplateArgs(initializer.templateArgs);
      return true;
    }
  }
  if (!initializer.isMethodCall && initializer.templateArgs.empty()) {
    const std::string experimentalVectorPath = experimentalVectorConstructorInferencePath(resolved);
    if (!experimentalVectorPath.empty() && ctx.sourceDefs.count(experimentalVectorPath) > 0) {
      auto defIt = ctx.sourceDefs.find(resolved);
      if (defIt != ctx.sourceDefs.end()) {
        std::vector<std::string> inferredArgs;
        std::string inferError;
        if (inferImplicitTemplateArgs(defIt->second,
                                      initializer,
                                      locals,
                                      params,
                                      SubstMap{},
                                      {},
                                      initializer.namespacePrefix,
                                      ctx,
                                      allowMathBare,
                                      inferredArgs,
                                      inferError) &&
            inferredArgs.size() == 1) {
          infoOut.typeName = canonicalVectorTypeIdentityPrefix() + "Vector";
          infoOut.typeTemplateArg = joinTemplateArgs(inferredArgs);
          return true;
        }
        if (!inferError.empty()) {
          handledOut = true;
          return false;
        }
        defIt = ctx.sourceDefs.find(resolved);
        if (defIt == ctx.sourceDefs.end()) {
          handledOut = true;
          return false;
        }
        for (const auto &transform : defIt->second.transforms) {
          if (transform.name != "return" || transform.templateArgs.size() != 1) {
            continue;
          }
          std::string valueType;
          if (extractCollectionVectorValueTypeFromTypeText(transform.templateArgs.front(), valueType)) {
            infoOut.typeName = canonicalVectorTypeIdentityPrefix() + "Vector";
            infoOut.typeTemplateArg = valueType;
            return true;
          }
        }
      }
    }
    if (isCollectionVectorConstructorHelperPath(resolved)) {
      auto defIt = ctx.sourceDefs.find(resolved);
      if (defIt != ctx.sourceDefs.end()) {
        std::vector<std::string> inferredArgs;
        std::string inferError;
        if (inferImplicitTemplateArgs(defIt->second,
                                      initializer,
                                      locals,
                                      params,
                                      SubstMap{},
                                      {},
                                      initializer.namespacePrefix,
                                      ctx,
                                      allowMathBare,
                                      inferredArgs,
                                      inferError) &&
            inferredArgs.size() == 1) {
          infoOut.typeName = canonicalVectorTypeIdentityPrefix() + "Vector";
          infoOut.typeTemplateArg = joinTemplateArgs(inferredArgs);
          return true;
        }
        if (!inferError.empty()) {
          handledOut = true;
          return false;
        }
        defIt = ctx.sourceDefs.find(resolved);
        if (defIt == ctx.sourceDefs.end()) {
          handledOut = true;
          return false;
        }
        for (const auto &transform : defIt->second.transforms) {
          if (transform.name != "return" || transform.templateArgs.size() != 1) {
            continue;
          }
          std::string valueType;
          if (extractCollectionVectorValueTypeFromTypeText(transform.templateArgs.front(), valueType)) {
            infoOut.typeName = canonicalVectorTypeIdentityPrefix() + "Vector";
            infoOut.typeTemplateArg = valueType;
            return true;
          }
        }
      }
    }
  }
  if (resolved.empty()) {
    return false;
  }

  auto defIt = ctx.sourceDefs.find(resolved);
  if (defIt == ctx.sourceDefs.end()) {
    return false;
  }
  if (isStructDefinition(defIt->second)) {
    infoOut.typeName = resolved;
    infoOut.typeTemplateArg.clear();
    return true;
  }

  std::vector<std::string> effectiveTemplateArgs = initializer.templateArgs;
  if (effectiveTemplateArgs.empty() && !defIt->second.templateArgs.empty()) {
    std::string inferError;
    if (!inferImplicitTemplateArgs(defIt->second,
                                   initializer,
                                   locals,
                                   params,
                                   SubstMap{},
                                   {},
                                   initializer.namespacePrefix,
                                   ctx,
                                   allowMathBare,
                                   effectiveTemplateArgs,
                                   inferError)) {
      if (!inferError.empty()) {
        handledOut = true;
        return false;
      }
      effectiveTemplateArgs.clear();
    }
  }

  std::unordered_set<std::string> allowedParams(defIt->second.templateArgs.begin(), defIt->second.templateArgs.end());
  SubstMap returnTypeMapping;
  if (effectiveTemplateArgs.size() == defIt->second.templateArgs.size()) {
    returnTypeMapping.reserve(effectiveTemplateArgs.size());
    for (size_t i = 0; i < effectiveTemplateArgs.size(); ++i) {
      returnTypeMapping.emplace(defIt->second.templateArgs[i], effectiveTemplateArgs[i]);
    }
  }
  for (const auto &transform : defIt->second.transforms) {
    if (transform.name != "return" || transform.templateArgs.size() != 1) {
      continue;
    }
    const std::string &returnType = transform.templateArgs.front();
    if (returnType == "auto") {
      break;
    }
    std::string resolvedError;
    ResolvedType resolvedReturnType =
        resolveTypeString(returnType, returnTypeMapping, allowedParams, defIt->second.namespacePrefix, ctx,
                          resolvedError);
    if (!resolvedError.empty() || !resolvedReturnType.concrete || resolvedReturnType.text.empty()) {
      break;
    }
    std::string base;
    std::string argText;
    if (splitTemplateTypeName(resolvedReturnType.text, base, argText) && !base.empty()) {
      infoOut.typeName = base;
      infoOut.typeTemplateArg = argText;
    } else {
      infoOut.typeName = resolvedReturnType.text;
      infoOut.typeTemplateArg.clear();
    }
    return true;
  }
  return inferDefinitionReturnBindingForTemplatedFallback(defIt->second, allowMathBare, ctx, infoOut);
}


} // namespace primec
