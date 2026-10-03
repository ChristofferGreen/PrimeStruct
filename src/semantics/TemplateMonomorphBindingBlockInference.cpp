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

bool inferBlockBodyBindingTypeForMonomorph(const Expr &initializer,
                                           const std::vector<semantics::ParameterInfo> &params,
                                           const LocalTypeMap &locals,
                                           bool allowMathBare,
                                           Context &ctx,
                                           semantics::BindingInfo &infoOut) {
  if (initializer.kind != Expr::Kind::Call || (!initializer.hasBodyArguments && initializer.bodyArguments.empty())) {
    return false;
  }
  const std::string resolved = resolveCalleePath(initializer, initializer.namespacePrefix, ctx);
  if (ctx.sourceDefs.count(resolved) > 0) {
    return false;
  }
  if (!initializer.args.empty() || !initializer.templateArgs.empty() || semantics::hasNamedArguments(initializer.argNames)) {
    return false;
  }
  if (initializer.bodyArguments.empty()) {
    return false;
  }

  LocalTypeMap blockLocals = locals;
  const Expr *valueExpr = nullptr;
  bool sawReturn = false;
  for (const auto &bodyExpr : initializer.bodyArguments) {
    if (bodyExpr.isBinding) {
      semantics::BindingInfo binding;
      if (extractExplicitBindingType(bodyExpr, binding)) {
        if (binding.typeName == "auto" && bodyExpr.args.size() == 1 &&
            inferBindingTypeForMonomorph(bodyExpr.args.front(), params, blockLocals, allowMathBare, ctx, binding)) {
          blockLocals[bodyExpr.name] = binding;
        } else {
          blockLocals[bodyExpr.name] = binding;
        }
      } else if (bodyExpr.args.size() == 1) {
        if (inferBindingTypeForMonomorph(bodyExpr.args.front(), params, blockLocals, allowMathBare, ctx, binding)) {
          blockLocals[bodyExpr.name] = binding;
        }
      }
      continue;
    }
    if (semantics::isReturnCall(bodyExpr)) {
      if (bodyExpr.args.size() != 1) {
        return false;
      }
      valueExpr = &bodyExpr.args.front();
      sawReturn = true;
      continue;
    }
    if (!sawReturn) {
      valueExpr = &bodyExpr;
    }
  }
  if (!valueExpr) {
    return false;
  }
  return inferBindingTypeForMonomorph(*valueExpr, params, blockLocals, allowMathBare, ctx, infoOut);
}

} // namespace primec
