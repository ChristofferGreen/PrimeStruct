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

std::string canonicalizeExperimentalCollectionResolvedPath(std::string path) {
  return stripKeyValueConstructorSuffixes(std::move(path));
}

bool isExperimentalMapEntryArgument(const Expr &argExpr,
                                    const std::vector<semantics::ParameterInfo> &params,
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
  semantics::BindingInfo argInfo;
  if (!inferBindingTypeForMonomorph(argExpr, params, locals, allowMathBare, ctx, argInfo)) {
    return false;
  }
  std::string argTypeText = argInfo.typeName;
  if (!argInfo.typeTemplateArg.empty()) {
    argTypeText += "<" + argInfo.typeTemplateArg + ">";
  }
  std::string normalizedArgType = semantics::normalizeBindingTypeName(argTypeText);
  if (!normalizedArgType.empty() && normalizedArgType.front() == '/') {
    normalizedArgType.erase(normalizedArgType.begin());
  }
  return isExperimentalKeyValueEntryBackingTypeName(normalizedArgType);
}

bool inferExperimentalCollectionConstructorTemplateArgs(const std::string &originalPath,
                                                        const std::string &helperPath,
                                                        Expr &valueExpr,
                                                        const LocalTypeMap &locals,
                                                        const std::vector<semantics::ParameterInfo> &params,
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
                                                         const std::vector<semantics::ParameterInfo> &params,
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
                                                       const std::vector<semantics::ParameterInfo> &params,
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
