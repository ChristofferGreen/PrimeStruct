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
#include "TemplateMonomorphUsings.h"

namespace primec {

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
