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


void rewriteDefinitionExperimentalKeyValueConstructorValue(Expr &valueExpr,
                                                           LocalTypeMap &locals,
                                                           std::vector<ParameterInfo> &params,
                                                           const SubstMap &mapping,
                                                           const std::unordered_set<std::string> &allowedParams,
                                                           const std::string &namespacePrefix,
                                                           Context &ctx,
                                                           bool allowMathBare,
                                                           std::string &error) {
  (void)rewriteCanonicalExperimentalKeyValueConstructorExpr(
      valueExpr, locals, params, mapping, allowedParams, namespacePrefix, ctx, allowMathBare, error);
}

void rewriteDefinitionExperimentalVectorConstructorValue(Expr &valueExpr,
                                                         LocalTypeMap &locals,
                                                         std::vector<ParameterInfo> &params,
                                                         const SubstMap &mapping,
                                                         const std::unordered_set<std::string> &allowedParams,
                                                         const std::string &namespacePrefix,
                                                         Context &ctx,
                                                         bool allowMathBare,
                                                         std::string &error) {
  (void)rewriteCanonicalExperimentalVectorConstructorExpr(
      valueExpr, locals, params, mapping, allowedParams, namespacePrefix, ctx, allowMathBare, error);
}

void rewriteDefinitionExperimentalVectorReturnConstructors(Expr &candidate,
                                                           LocalTypeMap &locals,
                                                           std::vector<ParameterInfo> &params,
                                                           const SubstMap &mapping,
                                                           const std::unordered_set<std::string> &allowedParams,
                                                           const std::string &namespacePrefix,
                                                           Context &ctx,
                                                           bool allowMathBare,
                                                           std::string &error) {
  rewriteExperimentalConstructorReturnTree(candidate, [&](Expr &valueExpr) {
    rewriteDefinitionExperimentalVectorConstructorValue(
        valueExpr, locals, params, mapping, allowedParams, namespacePrefix, ctx, allowMathBare, error);
  });
}

void rewriteDefinitionExperimentalKeyValueReturnConstructors(Expr &candidate,
                                                             LocalTypeMap &locals,
                                                             std::vector<ParameterInfo> &params,
                                                             const SubstMap &mapping,
                                                             const std::unordered_set<std::string> &allowedParams,
                                                             const std::string &namespacePrefix,
                                                             Context &ctx,
                                                             bool allowMathBare,
                                                             std::string &error) {
  rewriteExperimentalConstructorReturnTree(candidate, [&](Expr &valueExpr) {
    rewriteDefinitionExperimentalKeyValueConstructorValue(
        valueExpr, locals, params, mapping, allowedParams, namespacePrefix, ctx, allowMathBare, error);
  });
}

bool rewriteDefinitionExperimentalReturnConstructors(Expr &expr,
                                                     const ExperimentalCollectionReturnRewritePlan &plan,
                                                     LocalTypeMap &locals,
                                                     std::vector<ParameterInfo> &params,
                                                     const SubstMap &mapping,
                                                     const std::unordered_set<std::string> &allowedParams,
                                                     const std::string &namespacePrefix,
                                                     Context &ctx,
                                                     bool allowMathBare,
                                                     std::string &error) {
  return rewriteDefinitionReturnConstructors(
      expr,
      plan,
      [&](Expr &candidate) {
        rewriteDefinitionExperimentalVectorReturnConstructors(
            candidate, locals, params, mapping, allowedParams, namespacePrefix, ctx, allowMathBare, error);
      },
      [&](Expr &candidate) {
        rewriteDefinitionExperimentalKeyValueReturnConstructors(
            candidate, locals, params, mapping, allowedParams, namespacePrefix, ctx, allowMathBare, error);
      },
      error);
}


} // namespace primec
