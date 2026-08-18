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


std::string canonicalizeExperimentalCollectionResolvedPath(std::string path) {
  return stripKeyValueConstructorSuffixes(std::move(path));
}

bool isExperimentalMapEntryArgument(const Expr &argExpr,
                                    const std::vector<ParameterInfo> &params,
                                    const LocalTypeMap &locals,
                                    bool allowMathBare,
                                    const std::string &namespacePrefix,
                                    Context &ctx) {
  if (argExpr.isSpread) {
    return true;
  }
  const std::string resolvedArgPath =
      canonicalizeExperimentalCollectionResolvedPath(resolveCalleePath(argExpr, namespacePrefix, ctx));
  if (isExperimentalKeyValueConstructorMemberPathLocal(resolvedArgPath, "entry")) {
    return true;
  }
  BindingInfo argInfo;
  if (!inferBindingTypeForMonomorph(argExpr, params, locals, allowMathBare, ctx, argInfo)) {
    return false;
  }
  std::string argTypeText = argInfo.typeName;
  if (!argInfo.typeTemplateArg.empty()) {
    argTypeText += "<" + argInfo.typeTemplateArg + ">";
  }
  std::string normalizedArgType = normalizeBindingTypeName(argTypeText);
  if (!normalizedArgType.empty() && normalizedArgType.front() == '/') {
    normalizedArgType.erase(normalizedArgType.begin());
  }
  return isExperimentalKeyValueEntryBackingTypeName(normalizedArgType);
}

bool inferExperimentalCollectionConstructorTemplateArgs(const std::string &originalPath,
                                                        const std::string &helperPath,
                                                        Expr &valueExpr,
                                                        const LocalTypeMap &locals,
                                                        const std::vector<ParameterInfo> &params,
                                                        const SubstMap &mapping,
                                                        const std::unordered_set<std::string> &allowedParams,
                                                        const std::string &namespacePrefix,
                                                        Context &ctx,
                                                        bool allowMathBare,
                                                        std::string &error) {
  if (!valueExpr.templateArgs.empty()) {
    return true;
  }
  auto defIt = ctx.sourceDefs.find(helperPath);
  if (defIt == ctx.sourceDefs.end()) {
    return true;
  }
  std::vector<std::string> inferredArgs;
  std::string inferError;
  if (inferImplicitTemplateArgs(defIt->second,
                                valueExpr,
                                locals,
                                params,
                                mapping,
                                allowedParams,
                                namespacePrefix,
                                ctx,
                                allowMathBare,
                                inferredArgs,
                                inferError)) {
    valueExpr.templateArgs = std::move(inferredArgs);
    return true;
  }
  if (inferError.empty()) {
    return true;
  }
  error = inferError;
  const size_t helperPos = error.find(helperPath);
  if (helperPos != std::string::npos) {
    error.replace(helperPos, helperPath.size(), originalPath);
  }
  return false;
}

bool isCanonicalMapConstructorRewriteSourcePath(std::string_view originalPath) {
  const primec::StdlibSurfaceMetadata *metadata =
      keyValueConstructorSurfaceMetadataLocal();
  if (metadata == nullptr) {
    return false;
  }
  if (originalPath == metadata->canonicalPath) {
    return true;
  }
  return std::any_of(
      metadata->importAliasSpellings.begin(),
      metadata->importAliasSpellings.end(),
      [&](std::string_view alias) {
        return alias.find('/') == std::string_view::npos &&
               originalPath == "/" + std::string(alias);
      });
}

bool rewriteCanonicalExperimentalKeyValueConstructorExpr(Expr &valueExpr,
                                                         const LocalTypeMap &locals,
                                                         const std::vector<ParameterInfo> &params,
                                                         const SubstMap &mapping,
                                                         const std::unordered_set<std::string> &allowedParams,
                                                         const std::string &namespacePrefix,
                                                         Context &ctx,
                                                         bool allowMathBare,
                                                         std::string &error) {
  if (valueExpr.kind != Expr::Kind::Call || valueExpr.isBinding || valueExpr.isMethodCall) {
    return true;
  }
  const std::string originalPath = resolveCalleePath(valueExpr, namespacePrefix, ctx);
  std::string helperPath;
  if (isCanonicalMapConstructorRewriteSourcePath(originalPath) && !valueExpr.args.empty()) {
    const bool usesEntryArgs = std::all_of(valueExpr.args.begin(), valueExpr.args.end(), [&](const Expr &argExpr) {
      return isExperimentalMapEntryArgument(argExpr, params, locals, allowMathBare, namespacePrefix, ctx);
    });
    if (usesEntryArgs) {
      helperPath = experimentalKeyValueConstructorMemberPathLocal("map");
    } else {
      helperPath.clear();
    }
  }
  if (helperPath.empty() || ctx.sourceDefs.count(helperPath) == 0) {
    return true;
  }
  valueExpr.name = helperPath;
  valueExpr.namespacePrefix.clear();
  return inferExperimentalCollectionConstructorTemplateArgs(originalPath,
                                                            helperPath,
                                                            valueExpr,
                                                            locals,
                                                            params,
                                                            mapping,
                                                            allowedParams,
                                                            namespacePrefix,
                                                            ctx,
                                                            allowMathBare,
                                                            error);
}

bool rewriteCanonicalExperimentalVectorConstructorExpr(Expr &valueExpr,
                                                       const LocalTypeMap &locals,
                                                       const std::vector<ParameterInfo> &params,
                                                       const SubstMap &mapping,
                                                       const std::unordered_set<std::string> &allowedParams,
                                                       const std::string &namespacePrefix,
                                                       Context &ctx,
                                                       bool allowMathBare,
                                                       std::string &error) {
  if (valueExpr.kind != Expr::Kind::Call || valueExpr.isBinding || valueExpr.isMethodCall) {
    return true;
  }
  const std::string originalPath = resolveCalleePath(valueExpr, namespacePrefix, ctx);
  const std::string helperPath = experimentalVectorConstructorRewritePath(originalPath, valueExpr.args.size());
  if (helperPath.empty() || ctx.sourceDefs.count(helperPath) == 0) {
    return true;
  }
  valueExpr.name = helperPath;
  valueExpr.namespacePrefix.clear();
  return inferExperimentalCollectionConstructorTemplateArgs(originalPath,
                                                            helperPath,
                                                            valueExpr,
                                                            locals,
                                                            params,
                                                            mapping,
                                                            allowedParams,
                                                            namespacePrefix,
                                                            ctx,
                                                            allowMathBare,
                                                            error);
}


} // namespace primec
