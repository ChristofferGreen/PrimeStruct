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
#include "primec/support/CollectionHelperNames.h"
#include "TemplateMonomorphExpressionRewriteState.h"
#include "TemplateMonomorphExpressionRewriteInnerState.h"

namespace primec {

PhaseStatus rewriteExprPhase6([[maybe_unused]] Expr &expr, [[maybe_unused]] const SubstMap &mapping, [[maybe_unused]] const std::unordered_set<std::string> &allowedParams, [[maybe_unused]] const std::string &namespacePrefix, [[maybe_unused]] Context &ctx, [[maybe_unused]] std::string &error, [[maybe_unused]] const LocalTypeMap &locals, [[maybe_unused]] const std::vector<semantics::ParameterInfo> &params, RewriteExprState &st) {
  [[maybe_unused]] auto &allowMathBare = st.allowMathBare;
  [[maybe_unused]] auto &hadExplicitTemplateArgsOnEntry = st.hadExplicitTemplateArgsOnEntry;
  [[maybe_unused]] auto &isSyntheticSamePathSoaHelperTemplateCarryPath = st.isSyntheticSamePathSoaHelperTemplateCarryPath;
  [[maybe_unused]] auto &collectionHelperReceiverExpr = st.collectionHelperReceiverExpr;
  [[maybe_unused]] auto &mutableCollectionHelperReceiverExpr = st.mutableCollectionHelperReceiverExpr;
  [[maybe_unused]] auto &resolveExperimentalSoaVectorReceiverTemplateArgs = st.resolveExperimentalSoaVectorReceiverTemplateArgs;
  [[maybe_unused]] auto &resolvesExperimentalSoaVectorReceiver = st.resolvesExperimentalSoaVectorReceiver;
  [[maybe_unused]] auto &resolvesBorrowedExperimentalSoaVectorReceiver = st.resolvesBorrowedExperimentalSoaVectorReceiver;
  [[maybe_unused]] auto &resolvesConcreteExperimentalSoaVectorReceiver = st.resolvesConcreteExperimentalSoaVectorReceiver;
  [[maybe_unused]] auto &inferCollectionReceiverFamily = st.inferCollectionReceiverFamily;
  [[maybe_unused]] auto &isCanonicalSoaBorrowedWrapperHelper = st.isCanonicalSoaBorrowedWrapperHelper;
  [[maybe_unused]] auto &preferredBorrowedSoaWrapperPath = st.preferredBorrowedSoaWrapperPath;
  [[maybe_unused]] auto &preferCanonicalStdlibCollectionHelperPath = st.preferCanonicalStdlibCollectionHelperPath;
  [[maybe_unused]] auto &shouldDeferStdlibCollectionHelperTemplateRewrite = st.shouldDeferStdlibCollectionHelperTemplateRewrite;
  [[maybe_unused]] auto &rewriteNestedExperimentalKeyValueConstructorValue = st.rewriteNestedExperimentalKeyValueConstructorValue;
  [[maybe_unused]] auto &rewriteNestedExperimentalVectorConstructorValue = st.rewriteNestedExperimentalVectorConstructorValue;
  [[maybe_unused]] auto &rewriteKeyValueTargetValueForResolvedType = st.rewriteKeyValueTargetValueForResolvedType;
  [[maybe_unused]] auto &rewriteVectorTargetValueForResolvedType = st.rewriteVectorTargetValueForResolvedType;
  [[maybe_unused]] auto &allConcrete = st.allConcrete;
  if (!expr.isMethodCall && !expr.isBinding) {
    RewriteExprInnerState st2;
    if (rewriteExprReferencePhase1(expr, mapping, allowedParams, namespacePrefix, ctx, error, locals, params, st, st2) == PhaseStatus::Done) {
      return st2.result;
    }
    if (rewriteExprReferencePhase2(expr, mapping, allowedParams, namespacePrefix, ctx, error, locals, params, st, st2) == PhaseStatus::Done) {
      return st2.result;
    }
    if (rewriteExprReferencePhase3(expr, mapping, allowedParams, namespacePrefix, ctx, error, locals, params, st, st2) == PhaseStatus::Done) {
      return st2.result;
    }
    if (rewriteExprReferencePhase4(expr, mapping, allowedParams, namespacePrefix, ctx, error, locals, params, st, st2) == PhaseStatus::Done) {
      return st2.result;
    }
    if (rewriteExprReferencePhase5(expr, mapping, allowedParams, namespacePrefix, ctx, error, locals, params, st, st2) == PhaseStatus::Done) {
      return st2.result;
    }
    if (rewriteExprReferencePhase6(expr, mapping, allowedParams, namespacePrefix, ctx, error, locals, params, st, st2) == PhaseStatus::Done) {
      return st2.result;
    }
    if (rewriteExprReferencePhase7(expr, mapping, allowedParams, namespacePrefix, ctx, error, locals, params, st, st2) == PhaseStatus::Done) {
      return st2.result;
    }
    if (rewriteExprReferencePhase8(expr, mapping, allowedParams, namespacePrefix, ctx, error, locals, params, st, st2) == PhaseStatus::Done) {
      return st2.result;
    }
  }
  return PhaseStatus::Continue;
}

} // namespace primec
