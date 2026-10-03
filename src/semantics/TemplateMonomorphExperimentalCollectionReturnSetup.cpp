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

ExperimentalCollectionReturnRewritePlan inferExperimentalCollectionReturnRewritePlan(
    const Definition &def,
    const SubstMap &mapping,
    const std::unordered_set<std::string> &allowedParams,
    bool allowMathBare,
    Context &ctx) {
  ExperimentalCollectionReturnRewritePlan plan;
  for (const auto &transform : def.transforms) {
    if (transform.name != "return" || transform.templateArgs.size() != 1) {
      continue;
    }
    if (transform.templateArgs.front() != "auto") {
      plan.hasExplicitNonAutoReturn = true;
    }
    if (resolvesCollectionVectorValueTypeText(transform.templateArgs.front())) {
      plan.expectedCollectionVectorReturn = true;
    }
    if (resolvesExperimentalKeyValueTypeText(
            transform.templateArgs.front(), mapping, allowedParams, def.namespacePrefix, ctx)) {
      plan.expectedExperimentalKeyValueReturn = true;
    }
  }

  if (!plan.expectedCollectionVectorReturn && !plan.expectedExperimentalKeyValueReturn &&
      !plan.hasExplicitNonAutoReturn) {
    semantics::BindingInfo inferredReturnInfo;
    if (inferDefinitionReturnBindingForTemplatedFallback(def, allowMathBare, ctx, inferredReturnInfo)) {
      std::string inferredReturnType = inferredReturnInfo.typeName;
      if (!inferredReturnInfo.typeTemplateArg.empty()) {
        inferredReturnType += "<" + inferredReturnInfo.typeTemplateArg + ">";
      }
      plan.expectedCollectionVectorReturn = resolvesCollectionVectorValueTypeText(inferredReturnType);
      plan.expectedExperimentalKeyValueReturn = resolvesExperimentalKeyValueTypeText(
          inferredReturnType, mapping, allowedParams, def.namespacePrefix, ctx);
    }
  }

  return plan;
}

DefinitionReturnStatementSelection determineDefinitionReturnStatementSelection(const Definition &def) {
  DefinitionReturnStatementSelection selection;
  selection.implicitReturnStmtIndex = def.statements.size();
  for (size_t stmtIndex = 0; stmtIndex < def.statements.size(); ++stmtIndex) {
    if (semantics::isReturnCall(def.statements[stmtIndex])) {
      selection.sawExplicitReturn = true;
      break;
    }
    if (!def.statements[stmtIndex].isBinding) {
      selection.implicitReturnStmtIndex = stmtIndex;
    }
  }
  return selection;
}

} // namespace primec
