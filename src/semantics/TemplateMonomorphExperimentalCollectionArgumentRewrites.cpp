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

std::vector<ParameterInfo> buildExperimentalConstructorRewriteParams(const Definition &targetDef,
                                                                     bool allowMathBare,
                                                                     Context &ctx) {
  std::vector<ParameterInfo> callParams;
  if (!targetDef.parameters.empty()) {
    callParams.reserve(targetDef.parameters.size());
    for (const auto &paramExpr : targetDef.parameters) {
      ParameterInfo paramInfo;
      paramInfo.name = paramExpr.name;
      inferCallTargetBinding(paramExpr, allowMathBare, ctx, paramInfo.binding);
      if (paramExpr.args.size() == 1) {
        paramInfo.defaultExpr = &paramExpr.args.front();
      }
      callParams.push_back(std::move(paramInfo));
    }
    return callParams;
  }
  if (!isStructDefinition(targetDef)) {
    return callParams;
  }
  for (const auto &fieldExpr : targetDef.statements) {
    if (!fieldExpr.isBinding) {
      continue;
    }
    ParameterInfo fieldInfo;
    fieldInfo.name = fieldExpr.name;
    inferCallTargetBinding(fieldExpr, allowMathBare, ctx, fieldInfo.binding);
    if (fieldExpr.args.size() == 1) {
      fieldInfo.defaultExpr = &fieldExpr.args.front();
    }
    callParams.push_back(std::move(fieldInfo));
  }
  return callParams;
}

} // namespace primec
