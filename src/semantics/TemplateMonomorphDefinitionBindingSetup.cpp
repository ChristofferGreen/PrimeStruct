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

bool tryAppendDefinitionParameterBinding(Expr &param,
                                         bool allowMathBare,
                                         Context &ctx,
                                         LocalTypeMap &locals,
                                         std::vector<ParameterInfo> &paramsOut) {
  BindingInfo info;
  if (isCompileTimeTypeBinding(param)) {
    return false;
  }
  if (extractExplicitBindingType(param, info)) {
    if (info.typeName == "auto" && param.args.size() == 1 &&
        inferBindingTypeForMonomorph(param.args.front(), {}, {}, allowMathBare, ctx, info)) {
      locals[param.name] = info;
    } else {
      locals[param.name] = info;
    }
    ParameterInfo paramInfo;
    paramInfo.name = param.name;
    paramInfo.binding = info;
    if (param.args.size() == 1) {
      paramInfo.defaultExpr = &param.args.front();
    }
    paramsOut.push_back(std::move(paramInfo));
    return true;
  }
  if (!param.isBinding || param.args.size() != 1) {
    return false;
  }
  if (!inferBindingTypeForMonomorph(param.args.front(), {}, {}, allowMathBare, ctx, info)) {
    return false;
  }
  locals[param.name] = info;
  ParameterInfo paramInfo;
  paramInfo.name = param.name;
  paramInfo.binding = info;
  paramInfo.defaultExpr = &param.args.front();
  paramsOut.push_back(std::move(paramInfo));
  return true;
}

bool rewriteDefinitionParameters(std::vector<Expr> &parameters,
                                 const SubstMap &mapping,
                                 const std::unordered_set<std::string> &allowedParams,
                                 const std::string &namespacePrefix,
                                 Context &ctx,
                                 std::string &error,
                                 LocalTypeMap &locals,
                                 std::vector<ParameterInfo> &paramsOut,
                                 bool allowMathBare) {
  for (auto &param : parameters) {
    if (!rewriteExpr(param, mapping, allowedParams, namespacePrefix, ctx, error, locals, paramsOut, allowMathBare)) {
      return false;
    }
    tryAppendDefinitionParameterBinding(param, allowMathBare, ctx, locals, paramsOut);
  }
  return true;
}

void recordDefinitionStatementBindingLocal(Expr &stmt,
                                           const std::vector<ParameterInfo> &params,
                                           const LocalTypeMap &locals,
                                           bool allowMathBare,
                                           Context &ctx,
                                           LocalTypeMap &localsOut) {
  BindingInfo info;
  if (isCompileTimeTypeBinding(stmt)) {
    info.typeName = "type";
    localsOut[stmt.name] = info;
    return;
  }
  if (extractExplicitBindingType(stmt, info)) {
    if (info.typeName == "auto" && stmt.args.size() == 1 &&
        inferBindingTypeForMonomorph(stmt.args.front(), params, locals, allowMathBare, ctx, info)) {
      localsOut[stmt.name] = info;
    } else {
      localsOut[stmt.name] = info;
    }
    return;
  }
  if (!stmt.isBinding || stmt.args.size() != 1) {
    return;
  }
  if (inferBindingTypeForMonomorph(stmt.args.front(), params, locals, allowMathBare, ctx, info)) {
    localsOut[stmt.name] = info;
  }
}

} // namespace primec
